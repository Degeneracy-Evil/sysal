#include "pci_parser.hpp"
#include "parse_utils.hpp"
#include "parsed_facts.hpp"

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <string>
#include <unordered_map>

namespace sysal::detail
{

std::optional<PciSubsystem> parse_pci(const RawStore& raw, Diagnostics& diag)
{
    auto records = raw.get_all(RawSource::SysfsPci);
    if(records.empty())
    {
        return std::nullopt;
    }

    auto path_map = build_path_map(raw, RawSource::SysfsPci);
    auto device_addresses = extract_prefix_keys(path_map, "/sys/bus/pci/devices/");
    if(device_addresses.empty())
    {
        add_warning(diag, "PCI sysfs records exist but no device addresses parsed",
                    RawSource::SysfsPci);
        return std::nullopt;
    }

    PciSubsystem facts;
    for(const auto& dev_addr : device_addresses)
    {
        PciDevice device;
        device.address = parse_pci_address(dev_addr);

        const auto base = "/sys/bus/pci/devices/" + dev_addr + "/";

        auto vendor_it = path_map.find(base + "vendor");
        if(vendor_it != path_map.end())
        {
            device.vendor = Vendor{trim(vendor_it->second)};
        }

        auto device_it = path_map.find(base + "device");
        if(device_it != path_map.end())
        {
            device.device_name = DeviceName{trim(device_it->second)};
        }

        auto class_it = path_map.find(base + "class");
        if(class_it != path_map.end())
        {
            device.device_class = trim(class_it->second);
        }

        facts.devices.push_back(device);
    }

    return facts;
}

} // namespace sysal::detail
