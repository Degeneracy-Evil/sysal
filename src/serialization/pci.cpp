#include "serialization/pci.hpp"
#include "serialization/json_values.hpp"

#include <limits>
#include <utility>

namespace sysal::detail
{
    using json = nlohmann::json;

    namespace
    {

        [[nodiscard]] json pci_device_to_json(const PciDevice &pd)
        {
            json j = {
                {"address", pci_address_to_json(pd.address)},
                {"vendor", pd.vendor.value},
                {"device_name", pd.device_name.value},
                {"device_class", pd.device_class.value},
            };
            if(pd.numa_node)
            {
                j["numa_node"] = pd.numa_node->value();
            }
            if(!pd.physical_slot.empty())
            {
                j["physical_slot"] = pd.physical_slot;
            }
            if(!pd.firmware_label.empty())
            {
                j["firmware_label"] = pd.firmware_label;
            }
            if(!pd.current_link_speed.empty())
            {
                j["current_link_speed"] = pd.current_link_speed;
            }
            if(!pd.max_link_speed.empty())
            {
                j["max_link_speed"] = pd.max_link_speed;
            }
            if(pd.current_link_width)
            {
                j["current_link_width"] = *pd.current_link_width;
            }
            if(pd.max_link_width)
            {
                j["max_link_width"] = *pd.max_link_width;
            }
            if(pd.upstream_address)
                j["upstream_address"] = pci_address_to_json(*pd.upstream_address);
            if(pd.physical_function)
                j["physical_function"] = pci_address_to_json(*pd.physical_function);
            if(!pd.driver_name.value.empty())
                j["driver_name"] = pd.driver_name.value;
            if(!pd.local_cpus.empty())
            {
                j["local_cpus"] = json::array();
                for(const auto cpu : pd.local_cpus)
                    j["local_cpus"].push_back(cpu.value());
            }
            if(pd.maximum_virtual_functions)
                j["maximum_virtual_functions"] = *pd.maximum_virtual_functions;
            if(pd.enabled_virtual_functions)
                j["enabled_virtual_functions"] = *pd.enabled_virtual_functions;
            return j;
        }

        [[nodiscard]] PciDevice pci_device_from_json(const json &j)
        {
            PciDevice pd;
            pd.address = pci_address_from_json(j.at("address"));
            j.at("vendor").get_to(pd.vendor.value);
            j.at("device_name").get_to(pd.device_name.value);
            j.at("device_class").get_to(pd.device_class.value);
            if(j.contains("numa_node"))
            {
                pd.numa_node = NumaNodeId(uint32_from_json(j.at("numa_node"), "numa_node"));
            }
            pd.physical_slot = j.value("physical_slot", std::string{});
            pd.firmware_label = j.value("firmware_label", std::string{});
            pd.current_link_speed = j.value("current_link_speed", std::string{});
            pd.max_link_speed = j.value("max_link_speed", std::string{});
            if(j.contains("current_link_width"))
            {
                pd.current_link_width = uint32_from_json(j.at("current_link_width"), "current_link_width");
            }
            if(j.contains("max_link_width"))
            {
                pd.max_link_width = uint32_from_json(j.at("max_link_width"), "max_link_width");
            }
            if(j.contains("upstream_address"))
                pd.upstream_address = pci_address_from_json(j.at("upstream_address"));
            if(j.contains("physical_function"))
                pd.physical_function = pci_address_from_json(j.at("physical_function"));
            pd.driver_name = PciDriverName{j.value("driver_name", std::string{})};
            if(j.contains("local_cpus"))
                for(const auto &cpu : j.at("local_cpus").get_ref<const json::array_t &>())
                    pd.local_cpus.emplace_back(static_cast<std::uint32_t>(
                        checked_unsigned(cpu, std::numeric_limits<std::uint32_t>::max(), "PCI local CPU")));
            if(j.contains("maximum_virtual_functions"))
                pd.maximum_virtual_functions = static_cast<std::uint32_t>(checked_unsigned(
                    j.at("maximum_virtual_functions"), std::numeric_limits<std::uint32_t>::max(), "PCI maximum VFs"));
            if(j.contains("enabled_virtual_functions"))
                pd.enabled_virtual_functions = static_cast<std::uint32_t>(checked_unsigned(
                    j.at("enabled_virtual_functions"), std::numeric_limits<std::uint32_t>::max(), "PCI enabled VFs"));
            return pd;
        }

    } // namespace

    [[nodiscard]] json pci_to_json(const Pci &p)
    {
        json arr = json::array();
        for(const auto &dev : p.devices)
        {
            arr.push_back(pci_device_to_json(dev));
        }
        return json{{"devices", std::move(arr)}};
    }

    [[nodiscard]] Pci pci_from_json(const json &j)
    {
        Pci p;
        for(const auto &elem : j.at("devices"))
        {
            p.devices.push_back(pci_device_from_json(elem));
        }
        return p;
    }

} // namespace sysal::detail
