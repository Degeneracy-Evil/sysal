#include "parser/memory_topology.hpp"
#include "parser/parse_utils.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <utility>

namespace sysal::detail
{
    std::optional<std::uint32_t> edac_controller_index(std::string_view path)
    {
        constexpr std::string_view prefix = "/mc/mc";
        const auto start = path.find(prefix);
        if(start == std::string_view::npos)
            return std::nullopt;
        const auto number_start = start + prefix.size();
        const auto end = path.find('/', number_start);
        const auto last = end == std::string_view::npos ? path.size() : end;
        const auto number = parse_uint(path.substr(number_start, last - number_start));
        if(!number || *number > std::numeric_limits<std::uint32_t>::max())
            return std::nullopt;
        return static_cast<std::uint32_t>(*number);
    }

    void apply_memory_topology(Memory &memory, const RawStore &raw)
    {
        std::map<std::uint32_t, MemoryController> controllers;
        for(const auto *record : raw.get_all(RawSource::SysfsEdac))
        {
            if(record->status != CollectStatus::Success ||
               (record->path_or_command.find("/dimm") != std::string::npos ||
                record->path_or_command.find("/rank") != std::string::npos))
                continue;
            const auto index = edac_controller_index(record->path_or_command);
            if(!index)
                continue;
            auto &controller = controllers[*index];
            controller.index = *index;
            const auto name = extract_filename(record->path_or_command);
            const auto value = trim(record->payload);
            const auto number = parse_uint(value);
            if(name == "max_location")
                controller.max_location = parse_edac_location(value);
            else if(name == "mc_name")
                controller.name = hardware_text(value);
            else if(name == "size_mb" && number &&
                    *number <= std::numeric_limits<std::uint64_t>::max() / (1024ULL * 1024))
                controller.capacity = MemorySize{*number * 1024 * 1024};
            else if(name == "ce_count")
                controller.corrected_errors = number;
            else if(name == "ue_count")
                controller.uncorrected_errors = number;
            else if(name == "numa_node" && number && *number <= std::numeric_limits<std::uint32_t>::max())
                controller.numa_node = NumaNodeId{static_cast<std::uint32_t>(*number)};
            else if(name == "device")
                for(const auto &part : split(value, '/'))
                    if(const auto address = parse_pci_address(part))
                        controller.pci_address = address;
        }
        for(auto &[index, controller] : controllers)
            memory.controllers.push_back(std::move(controller));
        for(auto &dimm : memory.dimms)
            if(dimm.controller_index)
            {
                const auto controller =
                    std::find_if(memory.controllers.begin(), memory.controllers.end(),
                                 [&](const auto &item) { return item.index == *dimm.controller_index; });
                if(controller != memory.controllers.end())
                    dimm.numa_node = controller->numa_node;
            }
        for(auto &device : memory.edac_devices)
        {
            const auto controller =
                std::find_if(memory.controllers.begin(), memory.controllers.end(),
                             [&](const auto &item) { return item.index == device.controller_index; });
            if(controller != memory.controllers.end())
                device.numa_node = controller->numa_node;
        }
        for(const auto *record : raw.get_all(RawSource::Udevadm))
            if(record->status == CollectStatus::Success)
                for(const auto &line : split(record->payload, '\n'))
                {
                    const auto [key, value] = parse_kv(line, '=');
                    if(key == "E: MEMORY_ARRAY_LOCATION")
                        memory.reported_array_location = hardware_text(value);
                    else if(key == "E: MEMORY_ARRAY_EC_TYPE")
                        memory.reported_array_error_correction = hardware_text(value);
                    else if(key == "E: MEMORY_ARRAY_MAX_CAPACITY")
                    {
                        if(auto size = parse_uint(value))
                            memory.reported_array_max_capacity = MemorySize{*size};
                    }
                    else if(key == "E: MEMORY_ARRAY_NUM_DEVICES")
                    {
                        const auto count = parse_uint(value);
                        if(count && *count <= std::numeric_limits<std::uint32_t>::max())
                            memory.reported_slot_count = static_cast<std::uint32_t>(*count);
                    }
                }
        if(memory.dimm_inventory_source == "udev" && memory.reported_slot_count)
            memory.reported_slots_complete = memory.dimms.size() == *memory.reported_slot_count;
    }
} // namespace sysal::detail
