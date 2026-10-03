#include "resolver/storage_connections.hpp"
#include "parser/parse_utils.hpp"

namespace sysal::detail
{
    void resolve_storage_connections(Storage &storage, const Pci &pci)
    {
        for(const auto &device : pci.devices)
        {
            auto report = std::string_view{device.device_class.value};
            if(report.starts_with("0x") || report.starts_with("0X"))
                report.remove_prefix(2);
            const auto code = parse_hex(report);
            if(!code || *code > 0xffffff || (*code >> 16) != 1)
                continue;
            storage.controllers.push_back({device.address, static_cast<std::uint32_t>(*code), device.vendor,
                                           device.device_name, device.driver_name.value, device.numa_node});
        }
        for(auto &controller : storage.nvme_controllers)
            if(controller.pci_address)
                if(const auto *device = pci.find(*controller.pci_address))
                    controller.numa_node = device->numa_node;
        for(auto &host : storage.scsi_hosts)
            if(host.pci_address)
                if(const auto *device = pci.find(*host.pci_address))
                    host.numa_node = device->numa_node;
    }
} // namespace sysal::detail
