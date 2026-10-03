#include "parser/memory_topology.hpp"

#include "parser/parse_utils.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <sstream>
#include <utility>

namespace sysal::detail
{
    namespace
    {
        EdacLocationLayer location_layer(std::string_view name)
        {
            if(name == "branch")
                return EdacLocationLayer::Branch;
            if(name == "channel")
                return EdacLocationLayer::Channel;
            if(name == "slot")
                return EdacLocationLayer::Slot;
            if(name == "csrow")
                return EdacLocationLayer::ChipSelect;
            if(name == "memory")
                return EdacLocationLayer::AllMemory;
            return EdacLocationLayer::Unknown;
        }

        std::optional<EdacMemoryDevice> device_identity(std::string_view directory)
        {
            const auto controller = edac_controller_index(directory);
            if(!controller)
                return std::nullopt;
            const auto name = extract_filename(directory);
            const auto kind = name.starts_with("dimm")   ? EdacMemoryDeviceKind::Dimm
                              : name.starts_with("rank") ? EdacMemoryDeviceKind::Rank
                                                         : EdacMemoryDeviceKind::Unknown;
            if(kind == EdacMemoryDeviceKind::Unknown)
                return std::nullopt;
            const auto index = parse_uint(std::string_view{name}.substr(4));
            if(!index || *index > std::numeric_limits<std::uint32_t>::max())
                return std::nullopt;
            // Only direct controller children have this identity.
            const auto parent = directory.substr(0, directory.find_last_of('/'));
            if(extract_filename(parent) != "mc" + std::to_string(*controller))
                return std::nullopt;
            EdacMemoryDevice device;
            device.controller_index = *controller;
            device.index = EdacMemoryDeviceIndex{static_cast<std::uint32_t>(*index)};
            device.kind = kind;
            return device;
        }

        DimmInfo inventory_dimm(const EdacMemoryDevice &device, MemorySize size)
        {
            DimmInfo dimm;
            dimm.locator = device.label;
            dimm.bank_locator = device.location.report;
            dimm.size = size;
            dimm.present = size.value > 0;
            dimm.memory_type = device.memory_type;
            dimm.edac_mode = device.edac_mode;
            dimm.device_width = device.device_width;
            dimm.controller_index = device.controller_index;
            dimm.edac_device_index = device.index;
            dimm.edac_association = DimmEdacAssociation::EdacInventory;
            return dimm;
        }
    } // namespace

    EdacLocation parse_edac_location(std::string_view report)
    {
        EdacLocation location{trim(report), {}};
        std::istringstream input{location.report};
        std::string name;
        std::string value;
        while(input >> name)
        {
            if(!(input >> value))
                return {location.report, {}};
            const auto layer = location_layer(name);
            const auto index = parse_uint(value);
            if(layer == EdacLocationLayer::Unknown || !index || *index > std::numeric_limits<std::uint32_t>::max() ||
               std::any_of(location.coordinates.begin(), location.coordinates.end(),
                           [&](const auto &item) { return item.layer == layer; }))
                return {location.report, {}};
            location.coordinates.push_back({layer, static_cast<std::uint32_t>(*index)});
        }
        return location;
    }

    void apply_edac_inventory(Memory &memory, const RawStore &raw, std::string &memory_type)
    {
        std::map<std::string, EdacMemoryDevice> grouped;
        for(const auto *record : raw.get_all(RawSource::SysfsEdac))
        {
            if(record->status != CollectStatus::Success)
                continue;
            const auto &path = record->path_or_command;
            const auto slash = path.find_last_of('/');
            if(slash == std::string::npos)
                continue;
            const auto directory = path.substr(0, slash);
            const auto identity = device_identity(directory);
            if(!identity)
                continue;
            auto [entry, inserted] = grouped.try_emplace(directory, *identity);
            auto &device = entry->second;
            const auto field = path.substr(slash + 1);
            const auto value = hardware_text(record->payload);
            if(field == "dimm_label")
                device.label = value;
            else if(field == "dimm_location")
                device.location = parse_edac_location(record->payload);
            else if(field == "dimm_mem_type")
                device.memory_type = value;
            else if(field == "dimm_edac_mode")
                device.edac_mode = value;
            else if(field == "dimm_dev_type")
                device.device_width = value;
            else if(field == "size")
            {
                const auto size = parse_uint(trim(record->payload));
                if(size && *size <= std::numeric_limits<std::uint64_t>::max() / (1024ULL * 1024))
                    device.size = MemorySize{*size * 1024 * 1024};
            }
        }
        for(auto &[directory, device] : grouped)
            memory.edac_devices.push_back(std::move(device));
        std::sort(memory.edac_devices.begin(), memory.edac_devices.end(),
                  [](const auto &left, const auto &right)
                  {
                      if(left.controller_index != right.controller_index)
                          return left.controller_index < right.controller_index;
                      if(left.kind != right.kind)
                          return left.kind < right.kind;
                      return left.index.value() < right.index.value();
                  });
        const bool fallback = memory.dimms.empty();
        for(const auto &device : memory.edac_devices)
        {
            if(device.kind != EdacMemoryDeviceKind::Dimm || !device.size)
                continue;
            if(fallback)
            {
                memory.dimms.push_back(inventory_dimm(device, *device.size));
                if(device.size->value > 0 && !device.memory_type.empty())
                    memory_type = memory_type.empty()                 ? device.memory_type
                                  : memory_type == device.memory_type ? memory_type
                                                                      : "mixed";
                continue;
            }
            if(device.label.empty() || device.size->value == 0)
                continue;
            const auto matches_dimm = [&](const DimmInfo &dimm)
            { return dimm.present && dimm.locator == device.label && dimm.size == *device.size; };
            const auto matches_edac = [&](const EdacMemoryDevice &other) {
                return other.kind == EdacMemoryDeviceKind::Dimm && other.label == device.label &&
                       other.size == device.size;
            };
            if(std::count_if(memory.dimms.begin(), memory.dimms.end(), matches_dimm) != 1 ||
               std::count_if(memory.edac_devices.begin(), memory.edac_devices.end(), matches_edac) != 1)
                continue;
            auto dimm = std::find_if(memory.dimms.begin(), memory.dimms.end(), matches_dimm);
            dimm->edac_mode = device.edac_mode;
            dimm->device_width = device.device_width;
            dimm->controller_index = device.controller_index;
            dimm->edac_device_index = device.index;
            dimm->edac_association = DimmEdacAssociation::LabelAndCapacity;
        }
        if(fallback && !memory.dimms.empty())
            memory.dimm_inventory_source = "edac";
    }
} // namespace sysal::detail
