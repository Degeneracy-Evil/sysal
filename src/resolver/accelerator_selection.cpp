#include "resolver/accelerator_selection.hpp"
#include "parser/parse_utils.hpp"

#include <algorithm>
#include <cctype>
#include <tuple>

namespace sysal::detail
{
    namespace
    {
        std::optional<std::string_view> environment(const ExecutionContext &execution, std::string_view name)
        {
            for(const auto &[key, value] : execution.environment.entries)
                if(key == name)
                    return value;
            return std::nullopt;
        }

        void append_unique(DeviceIndices &target, const DeviceIndices &source)
        {
            for(const auto index : source)
                if(std::find(target.begin(), target.end(), index) == target.end())
                    target.push_back(index);
        }

        bool matches_uuid(const AcceleratorDevice &device, std::string_view token)
        {
            if(device.uuid && device.uuid->starts_with(token))
                return true;
            if(!device.parent_uuid || !device.gpu_instance_id || !device.compute_instance_id)
                return false;
            const auto legacy = "MIG-" + *device.parent_uuid + "/" + std::to_string(*device.gpu_instance_id) + "/" +
                                std::to_string(*device.compute_instance_id);
            return legacy == token;
        }

        enum class OrdinalOrder
        {
            Candidate,
            PhysicalId
        };

        DeviceIndices select_csv(const Accelerators &devices, const DeviceIndices &pool, const DeviceIndices &uuid_pool,
                                 std::string_view value, std::string_view variable, OrdinalOrder order,
                                 std::vector<std::string> &warnings)
        {
            DeviceIndices selected;
            if(trim(value).empty())
                return selected;
            for(const auto &part : split(value, ','))
            {
                const auto token = trim(part);
                if(token == "-1" || token == "none")
                    break;
                DeviceIndices matches;
                const bool uuid = token.starts_with("GPU-") || token.starts_with("MIG-");
                if(const auto ordinal = parse_uint(token))
                {
                    const auto number = ordinal.value();
                    if(order == OrdinalOrder::Candidate && number < pool.size())
                        matches.push_back(pool[static_cast<std::size_t>(number)]);
                    else if(order == OrdinalOrder::PhysicalId)
                        for(const auto index : pool)
                            if(devices.devices[index].id.value() == number)
                                matches.push_back(index);
                }
                else if(uuid)
                    for(const auto index : uuid_pool)
                        if(matches_uuid(devices.devices[index], token))
                            matches.push_back(index);
                if(matches.empty() || (uuid && matches.size() != 1))
                {
                    auto message = std::string{"[visibility] "};
                    message.append(variable).append(": invalid or ambiguous selector '").append(token);
                    message += "'; later entries ignored";
                    warnings.push_back(std::move(message));
                    break;
                }
                append_unique(selected, matches);
            }
            return selected;
        }

        struct SyclFilter
        {
            std::string backend;
            std::vector<std::string> selectors;
            bool exclude{};
        };

        std::optional<std::vector<SyclFilter>> parse_sycl(std::string_view value)
        {
            std::vector<SyclFilter> filters;
            bool discarding = false;
            for(const auto &clause : split(value, ';'))
            {
                auto text = trim(clause);
                for(auto &character : text)
                    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                const bool exclude = text.starts_with('!');
                if(exclude)
                    text.erase(0, 1);
                else if(discarding)
                    return std::nullopt;
                discarding = discarding || exclude;
                const auto parts = split(text, ':');
                if(parts.size() != 2)
                    return std::nullopt;
                const auto backend = trim(parts[0]);
                if(backend != "*" && backend != "level_zero" && backend != "opencl" && backend != "cuda" &&
                   backend != "hip" && backend != "native_cpu")
                    return std::nullopt;
                auto selectors = split(parts[1], ',');
                for(auto &selector : selectors)
                {
                    selector = trim(selector);
                    if(selector != "*" && selector != "gpu" && selector != "cpu" && !parse_uint(selector))
                        return std::nullopt; // Subdevice selectors need a subdevice model.
                }
                if(selectors.empty())
                    return std::nullopt;
                filters.push_back({backend, std::move(selectors), exclude});
            }
            return filters;
        }

        DeviceIndices select_sycl(const DeviceIndices &candidates, std::string_view value, bool runtime_known,
                                  std::vector<std::string> &warnings)
        {
            if(trim(value).empty())
                return {};
            const auto filters = parse_sycl(value);
            if(!filters)
            {
                warnings.push_back("[visibility] unsupported ONEAPI_DEVICE_SELECTOR; Intel selection left unchanged");
                return candidates;
            }
            const bool has_accept =
                std::any_of(filters->begin(), filters->end(), [](const auto &filter) { return !filter.exclude; });
            DeviceIndices selected = has_accept ? DeviceIndices{} : candidates;
            for(const auto &filter : *filters)
            {
                if(filter.backend != "*" && filter.backend != "level_zero" && filter.backend != "opencl")
                    continue;
                DeviceIndices matches;
                for(const auto &selector : filter.selectors)
                {
                    if(selector == "gpu" || selector == "*")
                        append_unique(matches, candidates);
                    else if(const auto ordinal = parse_uint(selector))
                    {
                        // Root Level Zero ordinals use its enumeration; other backends are not enumerated.
                        if(filter.backend != "level_zero" || !runtime_known)
                        {
                            warnings.push_back(
                                "[visibility] SYCL backend ordinal unavailable; Intel selection left unchanged");
                            return candidates;
                        }
                        if(*ordinal < candidates.size())
                            append_unique(matches, {candidates[static_cast<std::size_t>(*ordinal)]});
                    }
                }
                if(filter.exclude)
                    std::erase_if(selected, [&](const auto index)
                                  { return std::find(matches.begin(), matches.end(), index) != matches.end(); });
                else
                    append_unique(selected, matches);
            }
            return selected;
        }
    } // namespace

    std::optional<DeviceIndices> select_accelerator_environment(const Accelerators &devices, AcceleratorVendor vendor,
                                                                const DeviceIndices &candidates,
                                                                const ExecutionContext &execution, bool runtime_known,
                                                                std::vector<std::string> &warnings)
    {
        if(vendor == AcceleratorVendor::Nvidia)
        {
            const auto value = environment(execution, "CUDA_VISIBLE_DEVICES");
            if(!value || runtime_known)
                return std::nullopt;
            DeviceIndices physical;
            for(const auto index : candidates)
                if(!devices.devices[index].parent_uuid)
                    physical.push_back(index);
            const bool pci_order =
                environment(execution, "CUDA_DEVICE_ORDER") == std::optional<std::string_view>{"PCI_BUS_ID"};
            if(pci_order)
                std::sort(physical.begin(), physical.end(),
                          [&](const auto left, const auto right)
                          {
                              const auto &a = devices.devices[left].pci_address;
                              const auto &b = devices.devices[right].pci_address;
                              if(!a || !b)
                                  return a.has_value() != b.has_value() ? a.has_value() : left < right;
                              return std::tie(a->domain, a->bus, a->device, a->function) <
                                     std::tie(b->domain, b->bus, b->device, b->function);
                          });
            if(!candidates.empty())
                warnings.push_back(
                    "[visibility] CUDA runtime unavailable; visibility inferred from inventory and environment");
            return select_csv(devices, physical, candidates, *value, "CUDA_VISIBLE_DEVICES",
                              pci_order ? OrdinalOrder::Candidate : OrdinalOrder::PhysicalId, warnings);
        }
        auto selected = candidates;
        bool filtered = false;
        const auto apply = [&](std::string_view name)
        {
            if(const auto value = environment(execution, name))
            {
                selected = select_csv(devices, selected, selected, *value, name, OrdinalOrder::Candidate, warnings);
                filtered = true;
            }
        };
        if(vendor == AcceleratorVendor::Amd && !runtime_known)
        {
            apply("ROCR_VISIBLE_DEVICES");
            apply("HIP_VISIBLE_DEVICES");
            if(filtered)
                warnings.push_back("[visibility] HIP runtime unavailable; AMD ordinals use DRM inventory order");
        }
        if(vendor == AcceleratorVendor::Intel)
        {
            if(!runtime_known)
            {
                if(const auto mask = environment(execution, "ZE_AFFINITY_MASK");
                   mask && mask->find('.') != std::string_view::npos)
                    warnings.push_back(
                        "[visibility] Level Zero subdevice mask unavailable; Intel selection left unchanged");
                else
                    apply("ZE_AFFINITY_MASK");
            }
            if(const auto value = environment(execution, "ONEAPI_DEVICE_SELECTOR"))
            {
                selected = select_sycl(selected, *value, runtime_known, warnings);
                filtered = true;
            }
        }
        return filtered ? std::optional<DeviceIndices>{std::move(selected)} : std::nullopt;
    }
} // namespace sysal::detail
