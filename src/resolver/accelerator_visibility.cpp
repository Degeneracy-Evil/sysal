#include "resolver/accelerator_visibility.hpp"
#include "parser/parse_utils.hpp"

#include <algorithm>
#include <optional>
#include <tuple>
#include <unordered_set>

namespace sysal::detail
{
    namespace
    {
        std::optional<std::string> environment(const ExecutionContext &context, const std::string &name)
        {
            for(const auto &[key, value] : context.environment.entries)
                if(key == name)
                    return value;
            return std::nullopt;
        }

        bool uuid_matches(const AcceleratorDevice &device, const std::string &token)
        {
            if(device.uuid && device.uuid->starts_with(token))
                return true;
            if(device.parent_uuid && device.gpu_instance_id && device.compute_instance_id)
            {
                const auto legacy = "MIG-" + *device.parent_uuid + "/" + std::to_string(*device.gpu_instance_id) + "/" +
                                    std::to_string(*device.compute_instance_id);
                return legacy == token;
            }
            return false;
        }

        std::vector<std::size_t> select(const Accelerators &devices, const std::vector<std::size_t> &candidates,
                                        const std::string &value, const std::string &variable,
                                        std::vector<std::string> &warnings)
        {
            std::vector<std::size_t> selected;
            if(value.empty() || value == "-1" || value == "none")
                return selected;
            for(const auto &part : split(value, ','))
            {
                const auto token = trim(part);
                std::vector<std::size_t> matches;
                if(token == "*" || token == "all")
                    matches = candidates;
                else if(const auto ordinal = parse_uint(token); ordinal && *ordinal < candidates.size())
                    matches.push_back(candidates[static_cast<std::size_t>(*ordinal)]);
                else if(token.starts_with("GPU-") || token.starts_with("MIG-"))
                {
                    for(const auto index : candidates)
                        if(uuid_matches(devices.devices[index], token))
                            matches.push_back(index);
                }
                if(matches.empty() || ((token.starts_with("GPU-") || token.starts_with("MIG-")) && matches.size() != 1))
                {
                    auto message = std::string{"[visibility] "};
                    message += variable;
                    message += ": invalid or ambiguous selector '";
                    message += token;
                    message += "'; later entries ignored";
                    warnings.push_back(std::move(message));
                    break;
                }
                for(const auto match : matches)
                    if(std::find(selected.begin(), selected.end(), match) == selected.end())
                        selected.push_back(match);
            }
            return selected;
        }
    } // namespace

    void resolve_accelerator_visibility(Accelerators &devices, ExecutionContext &execution,
                                        std::vector<std::string> &warnings,
                                        const std::vector<std::pair<AcceleratorId, bool>> &runtime)
    {
        const auto legacy_ids = execution.visible_accelerator_ids;
        bool has_environment = false;
        for(const auto *name : {"CUDA_VISIBLE_DEVICES", "HIP_VISIBLE_DEVICES", "ROCR_VISIBLE_DEVICES",
                                "ZE_AFFINITY_MASK", "ONEAPI_DEVICE_SELECTOR"})
            has_environment = has_environment || environment(execution, name).has_value();
        if(!has_environment)
            for(const auto id : legacy_ids)
                if(std::none_of(devices.devices.begin(), devices.devices.end(),
                                [&](const auto &device) { return device.id == id; }))
                    warnings.push_back("[visibility_mismatch] accelerator_" + std::to_string(id.value()) +
                                       ": in_visible_accelerator_ids but accelerator does not exist in model");
        for(auto &device : devices.devices)
        {
            const bool has_children =
                device.uuid && std::any_of(devices.devices.begin(), devices.devices.end(),
                                           [&](const auto &child) { return child.parent_uuid == device.uuid; });
            device.visible_to_current_process = !has_children;
            if(!has_environment && !legacy_ids.empty())
                device.visible_to_current_process =
                    std::find(legacy_ids.begin(), legacy_ids.end(), device.id) != legacy_ids.end();
        }
        execution.accelerator_visibility_restricted = has_environment || !legacy_ids.empty();
        for(const auto *vendor : {"NVIDIA", "AMD", "Intel"})
        {
            std::vector<std::size_t> candidates;
            for(std::size_t index = 0; index < devices.devices.size(); ++index)
                if(devices.devices[index].vendor.value == vendor)
                    candidates.push_back(index);
            std::sort(candidates.begin(), candidates.end(), [&](const auto left, const auto right)
                      { return devices.devices[left].id.value() < devices.devices[right].id.value(); });
            std::vector<std::size_t> selected = candidates;
            bool restricted = false;
            std::vector<std::size_t> observed;
            for(const auto &[id, visible] : runtime)
                for(const auto index : candidates)
                    if(devices.devices[index].id == id)
                    {
                        restricted = true;
                        if(visible)
                            observed.push_back(index);
                    }
            const bool runtime_known = restricted;
            if(runtime_known)
                selected = std::move(observed);
            const auto apply = [&](const char *name)
            {
                const auto value = environment(execution, name);
                if(!value)
                    return;
                selected = select(devices, selected, *value, name, warnings);
                restricted = true;
            };
            if(std::string_view(vendor) == "NVIDIA")
            {
                if(const auto value = environment(execution, "CUDA_VISIBLE_DEVICES"); value && !runtime_known)
                {
                    // Numeric CUDA ordinals address physical GPUs; UUID selectors may address MIG instances.
                    std::vector<std::size_t> physical;
                    for(const auto index : candidates)
                        if(!devices.devices[index].parent_uuid)
                            physical.push_back(index);
                    if(environment(execution, "CUDA_DEVICE_ORDER") == std::optional<std::string>{"PCI_BUS_ID"})
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
                    selected.clear();
                    for(const auto &token : split(*value, ','))
                    {
                        const auto &pool = trim(token).starts_with("MIG-") ? candidates : physical;
                        std::vector<std::size_t> matches;
                        if(const auto ordinal = parse_uint(trim(token));
                           ordinal &&
                           environment(execution, "CUDA_DEVICE_ORDER") != std::optional<std::string>{"PCI_BUS_ID"})
                        {
                            for(const auto index : physical)
                                if(devices.devices[index].id.value() == *ordinal)
                                    matches.push_back(index);
                            if(matches.empty())
                                warnings.push_back("[visibility] CUDA_VISIBLE_DEVICES: invalid device ordinal '" +
                                                   trim(token) + "'");
                        }
                        else
                            matches = select(devices, pool, token, "CUDA_VISIBLE_DEVICES", warnings);
                        if(matches.empty())
                            break;
                        selected.insert(selected.end(), matches.begin(), matches.end());
                    }
                    restricted = true;
                }
            }
            else if(std::string_view(vendor) == "AMD")
            {
                if(!runtime_known)
                {
                    apply("ROCR_VISIBLE_DEVICES");
                    apply("HIP_VISIBLE_DEVICES");
                    if(restricted)
                        warnings.push_back(
                            "[visibility] HIP runtime unavailable; AMD ordinals use DRM inventory order");
                }
            }
            else
            {
                if(!runtime_known)
                    apply("ZE_AFFINITY_MASK");
                if(const auto value = environment(execution, "ONEAPI_DEVICE_SELECTOR"))
                {
                    std::vector<std::size_t> matching;
                    for(const auto &clause : split(*value, ';'))
                    {
                        const auto parts = split(clause, ':');
                        if(parts.size() != 3 || (parts[0] != "level_zero" && parts[0] != "opencl" && parts[0] != "*"))
                            continue;
                        if(parts[1] != "gpu" && parts[1] != "*")
                            continue;
                        const auto chosen = select(devices, selected, parts[2], "ONEAPI_DEVICE_SELECTOR", warnings);
                        matching.insert(matching.end(), chosen.begin(), chosen.end());
                    }
                    selected = std::move(matching);
                    restricted = true;
                }
            }
            if(!restricted)
                continue;
            for(const auto index : candidates)
            {
                auto &device = devices.devices[index];
                const bool direct = std::find(selected.begin(), selected.end(), index) != selected.end();
                const bool via_parent =
                    device.parent_uuid && std::any_of(selected.begin(), selected.end(), [&](const auto parent)
                                                      { return devices.devices[parent].uuid == device.parent_uuid; });
                const bool has_children =
                    device.uuid && std::any_of(devices.devices.begin(), devices.devices.end(),
                                               [&](const auto &child) { return child.parent_uuid == device.uuid; });
                device.visible_to_current_process = (direct || via_parent) && !has_children;
            }
        }
        execution.visible_accelerator_ids.clear();
        for(const auto &device : devices.devices)
            if(device.visible_to_current_process)
                execution.visible_accelerator_ids.push_back(device.id);
    }
} // namespace sysal::detail
