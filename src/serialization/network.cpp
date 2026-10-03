#include "serialization/network.hpp"
#include "serialization/json_values.hpp"
#include "serialization/rdma.hpp"

#include <limits>
#include <utility>

namespace sysal::detail
{
    using json = nlohmann::json;

    namespace
    {

        [[nodiscard]] json net_iface_to_json(const NetworkInterface &ni)
        {
            json j = {
                {"name", ni.name.value},
                {"mac", ni.mac.value},
                {"state", static_cast<std::uint32_t>(ni.state)},
                {"visible_to_current_process", ni.visible_to_current_process},
            };
            if(ni.speed)
            {
                j["speed"] = ni.speed->value;
            }
            json addrs = json::array();
            for(const auto &addr : ni.addresses)
            {
                addrs.push_back(addr.value);
            }
            j["addresses"] = std::move(addrs);
            if(ni.pci_address)
            {
                j["pci_address"] = pci_address_to_json(*ni.pci_address);
            }
            if(!ni.duplex.empty())
            {
                j["duplex"] = ni.duplex;
            }
            if(!ni.driver.empty())
            {
                j["driver"] = ni.driver;
            }
            if(!ni.physical_port_name.empty())
            {
                j["physical_port_name"] = ni.physical_port_name;
            }
            if(ni.mtu)
            {
                j["mtu"] = ni.mtu->value;
            }
            if(ni.carrier)
            {
                j["carrier"] = *ni.carrier;
            }
            if(ni.numa_node)
            {
                j["numa_node"] = ni.numa_node->value();
            }
            if(ni.interface_index)
            {
                j["interface_index"] = *ni.interface_index;
            }
            if(!ni.vendor.value.empty())
            {
                j["vendor"] = ni.vendor.value;
            }
            if(!ni.device_name.value.empty())
            {
                j["device_name"] = ni.device_name.value;
            }
            if(!ni.firmware_version.empty())
            {
                j["firmware_version"] = ni.firmware_version;
            }
            if(!ni.driver_version.empty())
            {
                j["driver_version"] = ni.driver_version;
            }
            if(!ni.interface_kind.empty())
            {
                j["interface_kind"] = ni.interface_kind;
            }
            if(!ni.bond_mode.empty())
            {
                j["bond_mode"] = ni.bond_mode;
            }
            if(ni.permanent_mac)
            {
                j["permanent_mac"] = ni.permanent_mac->value;
            }
            if(ni.autonegotiation)
            {
                j["autonegotiation"] = *ni.autonegotiation;
            }
            if(ni.master)
            {
                j["master"] = ni.master->value;
            }
            if(ni.vlan_id)
            {
                j["vlan_id"] = *ni.vlan_id;
            }
            if(ni.vlan_parent)
            {
                j["vlan_parent"] = ni.vlan_parent->value;
            }
            if(!ni.lower_interfaces.empty())
            {
                j["lower_interfaces"] = json::array();
                for(const auto &item : ni.lower_interfaces)
                    j["lower_interfaces"].push_back(item.value);
            }
            if(!ni.supported_link_modes.empty())
                j["supported_link_modes"] = ni.supported_link_modes;
            if(!ni.advertised_link_modes.empty())
                j["advertised_link_modes"] = ni.advertised_link_modes;
            if(!ni.peer_link_modes.empty())
                j["peer_link_modes"] = ni.peer_link_modes;
            return j;
        }

        [[nodiscard]] NetworkInterface net_iface_from_json(const json &j)
        {
            NetworkInterface ni;
            j.at("name").get_to(ni.name.value);
            j.at("mac").get_to(ni.mac.value);
            ni.state = validate_enum(uint32_from_json(j.at("state"), "state"), InterfaceState::Unknown, "state");
            if(j.contains("speed"))
            {
                ni.speed = Bandwidth{uint64_from_json(j.at("speed"), "speed")};
            }
            if(j.contains("addresses"))
            {
                for(const auto &elem : j.at("addresses"))
                {
                    IpAddress ip;
                    ip.value = elem.get<std::string>();
                    ni.addresses.push_back(std::move(ip));
                }
            }
            if(j.contains("pci_address"))
            {
                ni.pci_address = pci_address_from_json(j.at("pci_address"));
            }
            ni.visible_to_current_process = j.at("visible_to_current_process").get<bool>();
            ni.duplex = j.value("duplex", std::string{});
            ni.driver = j.value("driver", std::string{});
            ni.physical_port_name = j.value("physical_port_name", std::string{});
            if(j.contains("mtu"))
            {
                ni.mtu = MemorySize{uint64_from_json(j.at("mtu"), "mtu")};
            }
            if(j.contains("carrier"))
            {
                ni.carrier = j.at("carrier").get<bool>();
            }
            if(j.contains("numa_node"))
            {
                ni.numa_node = NumaNodeId{uint32_from_json(j.at("numa_node"), "numa_node")};
            }
            if(j.contains("interface_index"))
            {
                ni.interface_index = uint32_from_json(j.at("interface_index"), "interface_index");
            }
            ni.vendor = Vendor{j.value("vendor", std::string{})};
            ni.device_name = DeviceName{j.value("device_name", std::string{})};
            ni.firmware_version = j.value("firmware_version", std::string{});
            ni.driver_version = j.value("driver_version", std::string{});
            ni.interface_kind = j.value("interface_kind", std::string{});
            ni.bond_mode = j.value("bond_mode", std::string{});
            if(j.contains("permanent_mac"))
            {
                ni.permanent_mac = MacAddress{j.at("permanent_mac").get<std::string>()};
            }
            if(j.contains("autonegotiation"))
            {
                ni.autonegotiation = j.at("autonegotiation").get<bool>();
            }
            if(j.contains("master"))
            {
                ni.master = InterfaceName{j.at("master").get<std::string>()};
            }
            if(j.contains("vlan_id"))
            {
                ni.vlan_id = uint32_from_json(j.at("vlan_id"), "vlan_id");
            }
            if(j.contains("vlan_parent"))
            {
                ni.vlan_parent = InterfaceName{j.at("vlan_parent").get<std::string>()};
            }
            if(j.contains("lower_interfaces"))
                for(const auto &item : j.at("lower_interfaces"))
                    ni.lower_interfaces.push_back(InterfaceName{item.get<std::string>()});
            if(j.contains("supported_link_modes"))
                ni.supported_link_modes = str_array_from_json(j.at("supported_link_modes"));
            if(j.contains("advertised_link_modes"))
                ni.advertised_link_modes = str_array_from_json(j.at("advertised_link_modes"));
            if(j.contains("peer_link_modes"))
                ni.peer_link_modes = str_array_from_json(j.at("peer_link_modes"));
            return ni;
        }

    } // namespace

    [[nodiscard]] json network_to_json(const Network &n)
    {
        json arr = json::array();
        for(const auto &iface : n.interfaces)
        {
            arr.push_back(net_iface_to_json(iface));
        }
        return json{{"interfaces", std::move(arr)}, {"rdma", detail::rdma_inventory_to_json(n.rdma)}};
    }

    [[nodiscard]] Network network_from_json(const json &j)
    {
        Network n;
        for(const auto &elem : j.at("interfaces"))
        {
            n.interfaces.push_back(net_iface_from_json(elem));
        }
        if(j.contains("rdma"))
            n.rdma = detail::rdma_inventory_from_json(j.at("rdma"));
        return n;
    }

} // namespace sysal::detail
