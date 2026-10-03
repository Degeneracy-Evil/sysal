#include "serialization/storage_connections.hpp"

#include "serialization/json_values.hpp"

#include <limits>
#include <utility>

namespace sysal::detail
{
    namespace
    {
        using json = nlohmann::json;

        std::uint32_t index_value(const json &j, const char *field)
        {
            return static_cast<std::uint32_t>(checked_unsigned(j, std::numeric_limits<std::uint32_t>::max(), field));
        }

        template <typename Device> void attachment_to_json(json &j, const Device &device)
        {
            if(device.pci_address)
                j["pci_address"] = pci_address_to_json(*device.pci_address);
            if(device.numa_node)
                j["numa_node"] = device.numa_node->value();
        }

        template <typename Device> void attachment_from_json(const json &j, Device &device)
        {
            if(j.contains("pci_address"))
                device.pci_address = pci_address_from_json(j.at("pci_address"));
            if(j.contains("numa_node"))
                device.numa_node = NumaNodeId{index_value(j.at("numa_node"), "NUMA node")};
        }
    } // namespace

    void storage_device_connections_to_json(nlohmann::json &j, const StorageDevice &device)
    {
        if(device.nvme_namespace)
        {
            const auto &ns = *device.nvme_namespace;
            json value{{"id", ns.id.value()}, {"nguid", ns.nguid}, {"eui", ns.eui}, {"controllers", json::array()}};
            for(const auto &controller : ns.controllers)
                value["controllers"].push_back(controller.value);
            j["nvme_namespace"] = std::move(value);
        }
        if(device.scsi_device)
        {
            const auto &scsi = *device.scsi_device;
            json value{{"address",
                        {{"host", scsi.address.host.value()},
                         {"channel", scsi.address.channel.value()},
                         {"target", scsi.address.target.value()},
                         {"lun", scsi.address.lun.value()}}},
                       {"state", scsi.state}};
            if(scsi.peripheral_type)
                value["peripheral_type"] = *scsi.peripheral_type;
            j["scsi_device"] = std::move(value);
        }
    }

    void storage_device_connections_from_json(const nlohmann::json &j, StorageDevice &device)
    {
        if(j.contains("nvme_namespace"))
        {
            const auto &value = j.at("nvme_namespace");
            const auto id = index_value(value.at("id"), "namespace ID");
            if(id == 0)
                throw SysalError(ErrorKind::DeserializationError, "NVMe namespace ID 不能为零");
            NvmeNamespaceInfo ns;
            ns.id = NvmeNamespaceId{id};
            ns.nguid = value.value("nguid", std::string{});
            ns.eui = value.value("eui", std::string{});
            if(value.contains("controllers"))
                for(const auto &controller : value.at("controllers").get_ref<const json::array_t &>())
                    ns.controllers.push_back(NvmeControllerName{controller.get<std::string>()});
            device.nvme_namespace = std::move(ns);
        }
        if(j.contains("scsi_device"))
        {
            const auto &value = j.at("scsi_device");
            const auto &address = value.at("address");
            ScsiDeviceInfo scsi;
            scsi.address = {
                ScsiHostNumber{index_value(address.at("host"), "SCSI host")},
                ScsiChannelId{index_value(address.at("channel"), "SCSI channel")},
                ScsiTargetId{index_value(address.at("target"), "SCSI target")},
                ScsiLun{checked_unsigned(address.at("lun"), std::numeric_limits<std::uint64_t>::max(), "SCSI LUN")}};
            scsi.state = value.value("state", std::string{});
            if(value.contains("peripheral_type"))
                scsi.peripheral_type = index_value(value.at("peripheral_type"), "SCSI peripheral type");
            device.scsi_device = std::move(scsi);
        }
    }

    void storage_connections_to_json(nlohmann::json &j, const Storage &storage)
    {
        if(!storage.controllers.empty())
        {
            j["controllers"] = json::array();
            for(const auto &controller : storage.controllers)
            {
                json value{{"pci_address", pci_address_to_json(controller.pci_address)},
                           {"class_code", controller.class_code},
                           {"vendor", controller.vendor.value},
                           {"model", controller.model.value},
                           {"driver", controller.driver}};
                if(controller.numa_node)
                    value["numa_node"] = controller.numa_node->value();
                j["controllers"].push_back(std::move(value));
            }
        }
        if(!storage.nvme_controllers.empty())
        {
            j["nvme_controllers"] = json::array();
            for(const auto &controller : storage.nvme_controllers)
            {
                json value{{"name", controller.name.value}, {"transport", controller.transport},
                           {"address", controller.address}, {"model", controller.model},
                           {"serial", controller.serial},   {"firmware_revision", controller.firmware_revision},
                           {"state", controller.state},     {"subsystem_nqn", controller.subsystem_nqn}};
                if(controller.controller_id)
                    value["controller_id"] = controller.controller_id->value();
                attachment_to_json(value, controller);
                j["nvme_controllers"].push_back(std::move(value));
            }
        }
        if(!storage.scsi_hosts.empty())
        {
            j["scsi_hosts"] = json::array();
            for(const auto &host : storage.scsi_hosts)
            {
                json value{{"number", host.number.value()},
                           {"proc_name", host.proc_name},
                           {"state", host.state},
                           {"supported_mode", host.supported_mode},
                           {"active_mode", host.active_mode}};
                if(host.ata_port)
                    value["ata_port"] = host.ata_port->value();
                attachment_to_json(value, host);
                j["scsi_hosts"].push_back(std::move(value));
            }
        }
    }

    void storage_connections_from_json(const nlohmann::json &j, Storage &storage)
    {
        if(j.contains("controllers"))
            for(const auto &value : j.at("controllers").get_ref<const json::array_t &>())
            {
                StorageController controller;
                controller.pci_address = pci_address_from_json(value.at("pci_address"));
                controller.class_code =
                    static_cast<std::uint32_t>(checked_unsigned(value.at("class_code"), 0xffffff, "PCI class"));
                controller.vendor = Vendor{value.value("vendor", std::string{})};
                controller.model = DeviceName{value.value("model", std::string{})};
                controller.driver = value.value("driver", std::string{});
                if(value.contains("numa_node"))
                    controller.numa_node = NumaNodeId{index_value(value.at("numa_node"), "NUMA node")};
                storage.controllers.push_back(std::move(controller));
            }
        if(j.contains("nvme_controllers"))
            for(const auto &value : j.at("nvme_controllers").get_ref<const json::array_t &>())
            {
                NvmeController controller;
                controller.name = NvmeControllerName{value.at("name").get<std::string>()};
                controller.transport = value.value("transport", std::string{});
                controller.address = value.value("address", std::string{});
                controller.model = value.value("model", std::string{});
                controller.serial = value.value("serial", std::string{});
                controller.firmware_revision = value.value("firmware_revision", std::string{});
                controller.state = value.value("state", std::string{});
                controller.subsystem_nqn = value.value("subsystem_nqn", std::string{});
                if(value.contains("controller_id"))
                    controller.controller_id = NvmeControllerId{static_cast<std::uint16_t>(
                        checked_unsigned(value.at("controller_id"), 0xffff, "NVMe controller ID"))};
                attachment_from_json(value, controller);
                storage.nvme_controllers.push_back(std::move(controller));
            }
        if(j.contains("scsi_hosts"))
            for(const auto &value : j.at("scsi_hosts").get_ref<const json::array_t &>())
            {
                ScsiHost host;
                host.number = ScsiHostNumber{index_value(value.at("number"), "SCSI host")};
                host.proc_name = value.value("proc_name", std::string{});
                host.state = value.value("state", std::string{});
                host.supported_mode = value.value("supported_mode", std::string{});
                host.active_mode = value.value("active_mode", std::string{});
                if(value.contains("ata_port"))
                    host.ata_port = AtaPortNumber{index_value(value.at("ata_port"), "ATA port")};
                attachment_from_json(value, host);
                storage.scsi_hosts.push_back(std::move(host));
            }
    }
} // namespace sysal::detail
