#include "serialization/rdma.hpp"

#include "serialization/json_values.hpp"

#include <limits>

namespace sysal::detail
{
    namespace
    {
        using json = nlohmann::json;

        template <typename Enum> Enum enumeration(const json &j, Enum maximum)
        {
            return static_cast<Enum>(checked_unsigned(j, static_cast<std::uint64_t>(maximum), "RDMA enum"));
        }

        template <typename Integer>
        void optional_integer(const json &j, const char *key, std::optional<Integer> &value,
                              std::uint64_t maximum = std::numeric_limits<Integer>::max())
        {
            if(j.contains(key))
                value = static_cast<Integer>(checked_unsigned(j.at(key), maximum, key));
        }

        template <typename Integer> void put(json &j, const char *key, const std::optional<Integer> &value)
        {
            if(value)
                j[key] = *value;
        }

        json interfaces_to_json(const std::vector<InterfaceName> &interfaces)
        {
            auto j = json::array();
            for(const auto &interface : interfaces)
                j.push_back(interface.value);
            return j;
        }

        std::vector<InterfaceName> interfaces_from_json(const json &j)
        {
            std::vector<InterfaceName> interfaces;
            for(const auto &interface : j.get_ref<const json::array_t &>())
                interfaces.push_back(InterfaceName{interface.get<std::string>()});
            return interfaces;
        }

        json port_to_json(const RdmaPort &port)
        {
            json j{{"number", port.number.value()},
                   {"state", static_cast<std::uint32_t>(port.state)},
                   {"physical_state", static_cast<std::uint32_t>(port.physical_state)},
                   {"link_layer", static_cast<std::uint32_t>(port.link_layer)},
                   {"rate_report", port.rate_report},
                   {"network_interfaces", interfaces_to_json(port.network_interfaces)}};
            if(port.rate)
                j["rate"] = port.rate->value;
            if(port.lid)
                j["lid"] = port.lid->value();
            if(port.subnet_manager_lid)
                j["subnet_manager_lid"] = port.subnet_manager_lid->value();
            put(j, "subnet_manager_sl", port.subnet_manager_sl);
            put(j, "lid_mask_count", port.lid_mask_count);
            put(j, "capability_mask", port.capability_mask);
            return j;
        }

        RdmaPort port_from_json(const json &j)
        {
            RdmaPort port;
            port.number = RdmaPortNumber{static_cast<std::uint32_t>(
                checked_unsigned(j.at("number"), std::numeric_limits<std::uint32_t>::max(), "RDMA port"))};
            if(j.contains("state"))
                port.state = enumeration(j.at("state"), RdmaPortState::ActiveDeferred);
            if(j.contains("physical_state"))
                port.physical_state = enumeration(j.at("physical_state"), RdmaPhysicalState::PhyTest);
            if(j.contains("link_layer"))
                port.link_layer = enumeration(j.at("link_layer"), RdmaLinkLayer::Ethernet);
            port.rate_report = j.value("rate_report", std::string{});
            if(j.contains("rate"))
                port.rate =
                    Bandwidth{checked_unsigned(j.at("rate"), std::numeric_limits<std::uint64_t>::max(), "rate")};
            for(const auto *key : {"lid", "subnet_manager_lid"})
                if(j.contains(key))
                {
                    const RdmaLid lid{static_cast<std::uint32_t>(
                        checked_unsigned(j.at(key), std::numeric_limits<std::uint32_t>::max(), key))};
                    if(std::string_view(key) == "lid")
                        port.lid = lid;
                    else
                        port.subnet_manager_lid = lid;
                }
            optional_integer(j, "subnet_manager_sl", port.subnet_manager_sl, 15);
            optional_integer(j, "lid_mask_count", port.lid_mask_count, 7);
            optional_integer(j, "capability_mask", port.capability_mask);
            if(j.contains("network_interfaces"))
                port.network_interfaces = interfaces_from_json(j.at("network_interfaces"));
            return port;
        }

        json device_to_json(const RdmaDevice &device)
        {
            json j{{"id", device.id.value()},
                   {"name", device.name.value},
                   {"node_type", static_cast<std::uint32_t>(device.node_type)},
                   {"description", device.description},
                   {"node_guid", device.node_guid.value},
                   {"system_image_guid", device.system_image_guid.value},
                   {"firmware_version", device.firmware_version},
                   {"driver", device.driver},
                   {"vendor", device.vendor.value},
                   {"device_name", device.device_name.value},
                   {"network_interfaces", interfaces_to_json(device.network_interfaces)},
                   {"ports", json::array()}};
            if(device.pci_address)
                j["pci_address"] = pci_address_to_json(*device.pci_address);
            if(device.numa_node)
                j["numa_node"] = device.numa_node->value();
            for(const auto &port : device.ports)
                j["ports"].push_back(port_to_json(port));
            return j;
        }

        RdmaDevice device_from_json(const json &j)
        {
            RdmaDevice device;
            device.id = RdmaDeviceId{static_cast<std::uint32_t>(
                checked_unsigned(j.at("id"), std::numeric_limits<std::uint32_t>::max(), "RDMA id"))};
            device.name = RdmaDeviceName{j.at("name").get<std::string>()};
            if(j.contains("node_type"))
                device.node_type = enumeration(j.at("node_type"), RdmaNodeType::UsnicUdp);
            device.description = j.value("description", std::string{});
            device.node_guid = RdmaGuid{j.value("node_guid", std::string{})};
            device.system_image_guid = RdmaGuid{j.value("system_image_guid", std::string{})};
            device.firmware_version = j.value("firmware_version", std::string{});
            device.driver = j.value("driver", std::string{});
            device.vendor = Vendor{j.value("vendor", std::string{})};
            device.device_name = DeviceName{j.value("device_name", std::string{})};
            if(j.contains("pci_address"))
                device.pci_address = pci_address_from_json(j.at("pci_address"));
            if(j.contains("numa_node"))
                device.numa_node = NumaNodeId{static_cast<std::uint32_t>(
                    checked_unsigned(j.at("numa_node"), std::numeric_limits<std::uint32_t>::max(), "RDMA NUMA"))};
            if(j.contains("network_interfaces"))
                device.network_interfaces = interfaces_from_json(j.at("network_interfaces"));
            if(j.contains("ports"))
                for(const auto &port : j.at("ports").get_ref<const json::array_t &>())
                    device.ports.push_back(port_from_json(port));
            return device;
        }
    } // namespace

    json rdma_inventory_to_json(const RdmaInventory &inventory)
    {
        json j{{"status", static_cast<std::uint32_t>(inventory.status)}, {"devices", json::array()}};
        if(inventory.failure)
            j["failure"] = static_cast<std::uint32_t>(*inventory.failure);
        for(const auto &device : inventory.devices)
            j["devices"].push_back(device_to_json(device));
        return j;
    }

    RdmaInventory rdma_inventory_from_json(const json &j)
    {
        RdmaInventory inventory;
        if(j.contains("status"))
        {
            inventory.status = enumeration(j.at("status"), CollectStatus::NotCollected);
            inventory.failure = std::nullopt;
        }
        if(j.contains("failure"))
            inventory.failure = enumeration(j.at("failure"), ReadFailure::LowPower);
        if(j.contains("devices"))
            for(const auto &device : j.at("devices").get_ref<const json::array_t &>())
                inventory.devices.push_back(device_from_json(device));
        return inventory;
    }
} // namespace sysal::detail
