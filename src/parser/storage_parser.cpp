#include "storage_parser.hpp"
#include "parse_utils.hpp"

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <string>
#include <unordered_map>

namespace sysal::detail
{

namespace
{

StorageKind classify_storage(const std::string& name)
{
    if(name.starts_with("nvme"))
    {
        return StorageKind::Nvme;
    }
    if(name.starts_with("sd"))
    {
        return StorageKind::Sata;
    }
    return StorageKind::Other;
}

} // namespace

std::optional<StorageSubsystem> parse_storage(const RawStore& raw, Diagnostics& diag)
{
    auto records = raw.get_all(RawSource::SysfsBlock);
    if(records.empty())
    {
        return std::nullopt;
    }

    auto path_map = build_path_map(raw, RawSource::SysfsBlock);
    auto device_names = extract_prefix_keys(path_map, "/sys/block/");
    if(device_names.empty())
    {
        add_warning(diag, "Block sysfs records exist but no devices parsed", RawSource::SysfsBlock);
        return std::nullopt;
    }

    StorageSubsystem facts;
    std::uint32_t index = 0;
    for(const auto& name : device_names)
    {
        StorageDevice device;
        device.id = StorageId{index};
        device.name = DeviceName{name};
        device.kind = classify_storage(name);

        const auto base = "/sys/block/" + name + "/";

        auto size_it = path_map.find(base + "size");
        if(size_it != path_map.end())
        {
            auto sectors = parse_uint(trim(size_it->second));
            if(sectors)
            {
                device.capacity = MemorySize{*sectors * 512ULL};
            }
        }

        auto model_it = path_map.find(base + "device/model");
        if(model_it != path_map.end())
        {
            auto model = trim(model_it->second);
            if(!model.empty())
            {
                device.name = DeviceName{model};
            }
        }

        auto device_it = path_map.find(base + "device");
        if(device_it != path_map.end())
        {
            auto pci_str = trim(device_it->second);
            if(!pci_str.empty())
            {
                device.pci_address = parse_pci_address(pci_str);
            }
        }

        facts.devices.push_back(device);
        ++index;
    }

    return facts;
}

} // namespace sysal::detail
