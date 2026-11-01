#include "parser/storage_classification.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace sysal::detail
{
    namespace
    {
        StorageTransport transport(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](char character)
                           { return static_cast<char>(std::tolower(static_cast<unsigned char>(character))); });
            for(const auto &[name, kind] :
                {std::pair{"pcie", StorageTransport::Pcie}, std::pair{"sata", StorageTransport::Sata},
                 std::pair{"sas", StorageTransport::Sas}, std::pair{"usb", StorageTransport::Usb},
                 std::pair{"tcp", StorageTransport::Tcp}, std::pair{"rdma", StorageTransport::Rdma},
                 std::pair{"fc", StorageTransport::FibreChannel}, std::pair{"iscsi", StorageTransport::Iscsi},
                 std::pair{"spi", StorageTransport::ParallelScsi}})
                if(value == name)
                    return kind;
            return StorageTransport::Unknown;
        }

        StoragePurpose purpose(const StorageDevice &device)
        {
            if(device.nvme_namespace && device.nvme_namespace->id.value() != 0)
                return StoragePurpose::Disk;
            if(device.scsi_device && device.scsi_device->peripheral_type)
                switch(*device.scsi_device->peripheral_type)
                {
                case 0x00: // SCSI direct access, RBC and zoned block devices.
                case 0x0e:
                case 0x14:
                    return StoragePurpose::Disk;
                case 0x04:
                case 0x05:
                case 0x07:
                    return StoragePurpose::Optical;
                case 0x01:
                case 0x02:
                case 0x03:
                case 0x06:
                case 0x08:
                case 0x09:
                case 0x0c:
                case 0x0d:
                case 0x11:
                case 0x1e:
                    return StoragePurpose::Other;
                default:
                    return StoragePurpose::Unknown;
                }
            if(device.driver == "virtio_blk")
                return StoragePurpose::Disk;
            return StoragePurpose::Unknown;
        }

    } // namespace

    void apply_storage_classification(Storage &storage)
    {
        for(auto &device : storage.devices)
        {
            auto &classification = device.classification;
            classification.purpose = StoragePurpose::Unknown;
            classification.protocol = StorageProtocol::Unknown;
            classification.transports.clear();
            // Partitions and virtual layers do not own their underlying device's protocol.
            const bool nvme = device.nvme_namespace && device.nvme_namespace->id.value() != 0;
            if(nvme && classification.layer == BlockDeviceLayer::Virtual)
                classification.layer = BlockDeviceLayer::Device; // Kernel NVMe multipath namespace.
            if(classification.layer == BlockDeviceLayer::Device || classification.layer == BlockDeviceLayer::Unknown)
            {
                classification.purpose = purpose(device);
                if(nvme)
                    classification.protocol = StorageProtocol::Nvme;
                else if(device.scsi_device)
                {
                    classification.protocol = StorageProtocol::Scsi;
                    const auto host =
                        std::find_if(storage.scsi_hosts.begin(), storage.scsi_hosts.end(), [&](const auto &entry)
                                     { return entry.number == device.scsi_device->address.host; });
                    if(host != storage.scsi_hosts.end() && host->ata_port)
                        classification.protocol = StorageProtocol::Ata;
                }
                const auto add_transport = [&](const std::string &value)
                {
                    const auto kind = transport(value);
                    if(kind != StorageTransport::Unknown)
                        classification.transports.push_back(kind);
                };
                add_transport(device.transport);
                if(nvme)
                    for(const auto &name : device.nvme_namespace->controllers)
                        for(const auto &controller : storage.nvme_controllers)
                            if(controller.name == name)
                                add_transport(controller.transport);
                std::sort(classification.transports.begin(), classification.transports.end());
                const auto end = std::unique(classification.transports.begin(), classification.transports.end());
                classification.transports.erase(end, classification.transports.end());
            }
        }
    }
} // namespace sysal::detail
