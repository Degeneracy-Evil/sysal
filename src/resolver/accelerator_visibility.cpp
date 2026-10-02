#include "resolver/accelerator_visibility.hpp"
#include "resolver/accelerator_selection.hpp"

#include <algorithm>

namespace sysal::detail
{
    namespace
    {
        bool has_selection_environment(const ExecutionContext &execution)
        {
            return std::any_of(execution.environment.entries.begin(), execution.environment.entries.end(),
                               [](const auto &entry)
                               {
                                   const auto &name = entry.first;
                                   return name == "CUDA_VISIBLE_DEVICES" || name == "HIP_VISIBLE_DEVICES" ||
                                          name == "ROCR_VISIBLE_DEVICES" || name == "ZE_AFFINITY_MASK" ||
                                          name == "ONEAPI_DEVICE_SELECTOR";
                               });
        }

        bool contains(const DeviceIndices &indices, std::size_t index)
        {
            return std::find(indices.begin(), indices.end(), index) != indices.end();
        }

        bool has_children(const Accelerators &devices, const AcceleratorDevice &parent)
        {
            return parent.uuid && std::any_of(devices.devices.begin(), devices.devices.end(),
                                              [&](const auto &child) { return child.parent_uuid == parent.uuid; });
        }

        std::optional<DeviceIndices> runtime_indices(const Accelerators &devices, AcceleratorVendor vendor,
                                                     const std::vector<RuntimeVisibility> &runtime)
        {
            for(const auto &observation : runtime)
            {
                if(observation.vendor != vendor)
                    continue;
                DeviceIndices indices;
                for(const auto id : observation.visible)
                    for(std::size_t index = 0; index < devices.devices.size(); ++index)
                        if(devices.devices[index].id == id)
                            indices.push_back(index);
                return indices;
            }
            return std::nullopt;
        }

        void apply_selection(Accelerators &devices, const DeviceIndices &candidates, const DeviceIndices &selected)
        {
            for(const auto index : candidates)
            {
                auto &device = devices.devices[index];
                const bool via_parent =
                    device.parent_uuid && std::any_of(selected.begin(), selected.end(), [&](const auto parent)
                                                      { return devices.devices[parent].uuid == device.parent_uuid; });
                device.visible_to_current_process =
                    (contains(selected, index) || via_parent) && !has_children(devices, device);
            }
        }
    } // namespace

    void resolve_accelerator_visibility(Accelerators &devices, ExecutionContext &execution,
                                        std::vector<std::string> &warnings,
                                        const std::vector<RuntimeVisibility> &runtime)
    {
        const auto legacy_ids = execution.visible_accelerator_ids;
        const bool has_environment = has_selection_environment(execution);
        if(!has_environment)
            for(const auto id : legacy_ids)
                if(std::none_of(devices.devices.begin(), devices.devices.end(),
                                [&](const auto &device) { return device.id == id; }))
                    warnings.push_back("[visibility_mismatch] accelerator_" + std::to_string(id.value()) +
                                       ": in_visible_accelerator_ids but accelerator does not exist in model");
        for(auto &device : devices.devices)
            device.visible_to_current_process =
                !has_children(devices, device) &&
                (has_environment || legacy_ids.empty() ||
                 std::find(legacy_ids.begin(), legacy_ids.end(), device.id) != legacy_ids.end());
        execution.accelerator_visibility_restricted = has_environment || !legacy_ids.empty();
        for(const auto vendor : {AcceleratorVendor::Nvidia, AcceleratorVendor::Amd, AcceleratorVendor::Intel})
        {
            DeviceIndices candidates;
            for(std::size_t index = 0; index < devices.devices.size(); ++index)
                if(devices.devices[index].vendor.value == vendor_name(vendor))
                    candidates.push_back(index);
            std::sort(candidates.begin(), candidates.end(), [&](const auto left, const auto right)
                      { return devices.devices[left].id.value() < devices.devices[right].id.value(); });
            auto observed = runtime_indices(devices, vendor, runtime);
            const auto selection = select_accelerator_environment(devices, vendor, observed.value_or(candidates),
                                                                  execution, observed.has_value(), warnings);
            if(selection)
                apply_selection(devices, candidates, *selection);
            else if(observed)
                apply_selection(devices, candidates, *observed);
        }
        execution.visible_accelerator_ids.clear();
        for(const auto &device : devices.devices)
            if(device.visible_to_current_process)
                execution.visible_accelerator_ids.push_back(device.id);
    }
} // namespace sysal::detail
