#include "parser/storage_connections.hpp"

#include "parser/parse_utils.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <utility>

namespace sysal::detail
{
    namespace
    {
        using Attributes = std::map<std::string, std::string>;
        using Inventory = std::map<std::string, Attributes>;

        Inventory inventory(const RawStore &raw, std::string_view root)
        {
            Inventory groups;
            for(const auto *record : raw.get_all(RawSource::SysfsStorageConnections))
            {
                if(record->status != CollectStatus::Success || !record->path_or_command.starts_with(root))
                    continue;
                const auto tail = std::string_view{record->path_or_command}.substr(root.size());
                const auto slash = tail.find('/');
                if(slash == std::string_view::npos || slash == 0)
                    continue;
                groups[std::string{tail.substr(0, slash)}][std::string{tail.substr(slash + 1)}] = trim(record->payload);
            }
            return groups;
        }

        std::string attribute(const Attributes &attributes, const char *key)
        {
            const auto value = attributes.find(key);
            return value == attributes.end() ? std::string{} : value->second;
        }

        std::optional<std::uint32_t> number(std::string_view value)
        {
            const auto parsed = parse_uint(value);
            if(!parsed || *parsed > std::numeric_limits<std::uint32_t>::max())
                return std::nullopt;
            return static_cast<std::uint32_t>(*parsed);
        }

        std::optional<std::uint32_t> numbered(std::string_view value, std::string_view prefix)
        {
            return value.starts_with(prefix) ? number(value.substr(prefix.size())) : std::nullopt;
        }

        std::optional<PciAddress> ancestor_pci(std::string_view path)
        {
            std::optional<PciAddress> address;
            for(const auto &part : split(path, '/'))
                if(const auto parsed = parse_pci_address(part))
                    address = parsed;
            return address;
        }

        std::optional<AtaPortNumber> ata_port(std::string_view path)
        {
            for(const auto &part : split(path, '/'))
                if(const auto port = numbered(part, "ata"))
                    return AtaPortNumber{*port};
            return std::nullopt;
        }

        std::optional<ScsiAddress> scsi_address(std::string_view path)
        {
            const auto parts = split(extract_filename(path), ':');
            if(parts.size() != 4)
                return std::nullopt;
            const auto host = number(parts[0]);
            const auto channel = number(parts[1]);
            const auto target = number(parts[2]);
            const auto lun = parse_uint(parts[3]);
            if(!host || !channel || !target || !lun)
                return std::nullopt;
            return ScsiAddress{ScsiHostNumber{*host}, ScsiChannelId{*channel}, ScsiTargetId{*target}, ScsiLun{*lun}};
        }

        std::string identifier(std::string_view value)
        {
            const auto cleaned = hardware_text(value);
            return cleaned.find_first_not_of("0 -:\t\n") == std::string::npos ? std::string{} : cleaned;
        }

        void attach_controller(NvmeNamespaceInfo &ns, std::string_view path, const Inventory &controllers)
        {
            for(const auto &[name, attributes] : controllers)
            {
                if(!numbered(name, "nvme"))
                    continue;
                const auto canonical = attribute(attributes, "sysfs_path");
                if(canonical.empty() || (path != canonical && !path.starts_with(canonical + "/")))
                    continue;
                if(std::none_of(ns.controllers.begin(), ns.controllers.end(),
                                [&](const auto &item) { return item.value == name; }))
                    ns.controllers.push_back(NvmeControllerName{name});
            }
        }

        void apply_block(StorageDevice &device, const Attributes &attributes, const Inventory &controllers)
        {
            const auto path = attribute(attributes, "device_path");
            if(const auto nsid = number(attribute(attributes, "nsid")); nsid && *nsid != 0)
            {
                NvmeNamespaceInfo ns;
                ns.id = NvmeNamespaceId{*nsid};
                ns.nguid = identifier(attribute(attributes, "nguid"));
                ns.eui = identifier(attribute(attributes, "eui"));
                attach_controller(ns, path, controllers);
                for(const auto &[field, target] : attributes)
                    if(field.starts_with("multipath/"))
                        attach_controller(ns, target, controllers);
                std::sort(ns.controllers.begin(), ns.controllers.end(),
                          [](const auto &left, const auto &right) { return left.value < right.value; });
                device.nvme_namespace = std::move(ns);
            }
            else if(const auto address = scsi_address(path))
            {
                ScsiDeviceInfo scsi;
                scsi.address = *address;
                scsi.peripheral_type = number(attribute(attributes, "device/type"));
                scsi.state = hardware_text(attribute(attributes, "device/state"));
                device.scsi_device = std::move(scsi);
            }
        }
    } // namespace

    void apply_storage_connections(Storage &storage, const RawStore &raw)
    {
        const auto nvme = inventory(raw, "/sys/class/nvme/");
        for(const auto &[name, attributes] : nvme)
        {
            if(!numbered(name, "nvme"))
                continue;
            NvmeController controller;
            controller.name = NvmeControllerName{name};
            controller.transport = hardware_text(attribute(attributes, "transport"));
            controller.address = hardware_text(attribute(attributes, "address"));
            controller.model = hardware_text(attribute(attributes, "model"));
            controller.serial = hardware_text(attribute(attributes, "serial"));
            controller.firmware_revision = hardware_text(attribute(attributes, "firmware_rev"));
            controller.state = hardware_text(attribute(attributes, "state"));
            controller.subsystem_nqn = hardware_text(attribute(attributes, "subsysnqn"));
            if(const auto id = number(attribute(attributes, "cntlid")); id && *id <= 0xffff)
                controller.controller_id = NvmeControllerId{static_cast<std::uint16_t>(*id)};
            controller.pci_address = ancestor_pci(attribute(attributes, "sysfs_path"));
            storage.nvme_controllers.push_back(std::move(controller));
        }
        for(const auto &[name, attributes] : inventory(raw, "/sys/class/scsi_host/"))
        {
            const auto index = numbered(name, "host");
            if(!index)
                continue;
            ScsiHost host;
            host.number = ScsiHostNumber{*index};
            host.proc_name = hardware_text(attribute(attributes, "proc_name"));
            host.state = hardware_text(attribute(attributes, "state"));
            host.supported_mode = hardware_text(attribute(attributes, "supported_mode"));
            host.active_mode = hardware_text(attribute(attributes, "active_mode"));
            const auto path = attribute(attributes, "sysfs_path");
            host.pci_address = ancestor_pci(path);
            host.ata_port = ata_port(path);
            storage.scsi_hosts.push_back(std::move(host));
        }
        std::sort(storage.scsi_hosts.begin(), storage.scsi_hosts.end(),
                  [](const auto &left, const auto &right) { return left.number.value() < right.number.value(); });
        const auto blocks = inventory(raw, "/sys/class/block/");
        for(auto &device : storage.devices)
            if(!device.partition_number)
                if(const auto attributes = blocks.find(device.name.value); attributes != blocks.end())
                    apply_block(device, attributes->second, nvme);
    }

} // namespace sysal::detail
