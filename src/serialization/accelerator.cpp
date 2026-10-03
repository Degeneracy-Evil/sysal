#include "serialization/accelerator.hpp"
#include "serialization/json_values.hpp"

#include <limits>
#include <utility>

namespace sysal::detail
{
    using json = nlohmann::json;

    namespace
    {

        [[nodiscard]] json accel_device_to_json(const AcceleratorDevice &d)
        {
            json j = {
                {"id", d.id.value()},
                {"kind", static_cast<std::uint32_t>(d.kind)},
                {"vendor", d.vendor.value},
                {"name", d.name.value},
                {"visible_to_current_process", d.visible_to_current_process},
            };
            if(d.pci_address)
            {
                j["pci_address"] = pci_address_to_json(*d.pci_address);
            }
            if(d.nearest_numa_node)
            {
                j["nearest_numa_node"] = d.nearest_numa_node->value();
            }
            if(d.memory_size)
            {
                j["memory_size"] = d.memory_size->value;
            }
            if(d.driver)
            {
                j["driver"] = d.driver->value();
            }
            if(d.uuid)
                j["uuid"] = *d.uuid;
            if(d.parent_uuid)
                j["parent_uuid"] = *d.parent_uuid;
            if(d.gpu_instance_id)
                j["gpu_instance_id"] = *d.gpu_instance_id;
            if(d.compute_instance_id)
                j["compute_instance_id"] = *d.compute_instance_id;
            return j;
        }

        [[nodiscard]] AcceleratorDevice accel_device_from_json(const json &j)
        {
            AcceleratorDevice d;
            d.id = AcceleratorId(uint32_from_json(j.at("id"), "id"));
            d.kind = validate_enum(uint32_from_json(j.at("kind"), "kind"), AcceleratorKind::Other, "kind");
            j.at("vendor").get_to(d.vendor.value);
            j.at("name").get_to(d.name.value);
            if(j.contains("pci_address"))
            {
                d.pci_address = pci_address_from_json(j.at("pci_address"));
            }
            if(j.contains("nearest_numa_node"))
            {
                d.nearest_numa_node = NumaNodeId(uint32_from_json(j.at("nearest_numa_node"), "nearest_numa_node"));
            }
            if(j.contains("memory_size"))
            {
                d.memory_size = MemorySize{uint64_from_json(j.at("memory_size"), "memory_size")};
            }
            if(j.contains("driver"))
            {
                d.driver = DriverId(uint32_from_json(j.at("driver"), "driver"));
            }
            d.visible_to_current_process = j.at("visible_to_current_process").get<bool>();
            if(j.contains("uuid"))
                d.uuid = j.at("uuid").get<std::string>();
            if(j.contains("parent_uuid"))
                d.parent_uuid = j.at("parent_uuid").get<std::string>();
            if(j.contains("gpu_instance_id"))
                d.gpu_instance_id = uint32_from_json(j.at("gpu_instance_id"), "gpu_instance_id");
            if(j.contains("compute_instance_id"))
                d.compute_instance_id = uint32_from_json(j.at("compute_instance_id"), "compute_instance_id");
            return d;
        }

    } // namespace

    [[nodiscard]] json accelerators_to_json(const Accelerators &a)
    {
        json arr = json::array();
        for(const auto &dev : a.devices)
        {
            arr.push_back(accel_device_to_json(dev));
        }
        return json{{"devices", std::move(arr)}};
    }

    [[nodiscard]] Accelerators accelerators_from_json(const json &j)
    {
        Accelerators a;
        for(const auto &elem : j.at("devices"))
        {
            a.devices.push_back(accel_device_from_json(elem));
        }
        return a;
    }

} // namespace sysal::detail
