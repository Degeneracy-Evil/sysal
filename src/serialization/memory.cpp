#include "serialization/memory.hpp"
#include "serialization/json_values.hpp"
#include "serialization/memory_topology.hpp"

#include <limits>
#include <utility>

namespace sysal::detail
{
    using json = nlohmann::json;

    namespace
    {
        [[nodiscard]] json memory_controller_to_json(const MemoryController &m)
        {
            json j{{"index", m.index}, {"name", m.name}};
            if(m.capacity)
                j["capacity"] = m.capacity->value;
            if(m.corrected_errors)
                j["corrected_errors"] = *m.corrected_errors;
            if(m.uncorrected_errors)
                j["uncorrected_errors"] = *m.uncorrected_errors;
            if(m.numa_node)
                j["numa_node"] = m.numa_node->value();
            if(m.pci_address)
                j["pci_address"] = pci_address_to_json(*m.pci_address);
            if(!m.max_location.report.empty() || !m.max_location.coordinates.empty())
                j["max_location"] = detail::edac_location_to_json(m.max_location);
            return j;
        }
        [[nodiscard]] MemoryController memory_controller_from_json(const json &j)
        {
            MemoryController m;
            m.index = uint32_from_json(j.at("index"), "index");
            m.name = j.value("name", std::string{});
            if(j.contains("capacity"))
                m.capacity = MemorySize{uint64_from_json(j.at("capacity"), "capacity")};
            if(j.contains("corrected_errors"))
                m.corrected_errors = uint64_from_json(j.at("corrected_errors"), "corrected_errors");
            if(j.contains("uncorrected_errors"))
                m.uncorrected_errors = uint64_from_json(j.at("uncorrected_errors"), "uncorrected_errors");
            if(j.contains("numa_node"))
                m.numa_node = NumaNodeId{uint32_from_json(j.at("numa_node"), "numa_node")};
            if(j.contains("pci_address"))
                m.pci_address = pci_address_from_json(j.at("pci_address"));
            if(j.contains("max_location"))
                m.max_location = detail::edac_location_from_json(j.at("max_location"));
            return m;
        }

        [[nodiscard]] json numa_memory_to_json(const NumaMemory &nm)
        {
            json j = {
                {"node", nm.node.value()},
                {"total", nm.total.value},
            };
            if(nm.available)
            {
                j["available"] = nm.available->value;
            }
            if(nm.free)
            {
                j["free"] = nm.free->value;
            }
            return j;
        }

        [[nodiscard]] NumaMemory numa_memory_from_json(const json &j)
        {
            NumaMemory nm;
            nm.node = NumaNodeId(uint32_from_json(j.at("node"), "node"));
            nm.total = MemorySize{uint64_from_json(j.at("total"), "total")};
            if(j.contains("available"))
            {
                nm.available = MemorySize{uint64_from_json(j.at("available"), "available")};
            }
            if(j.contains("free"))
            {
                nm.free = MemorySize{uint64_from_json(j.at("free"), "free")};
            }
            return nm;
        }

        [[nodiscard]] json dimm_info_to_json(const DimmInfo &d)
        {
            json j = {
                {"locator", d.locator},
                {"bank_locator", d.bank_locator},
                {"size", d.size.value},
                {"present", d.present},
            };
            if(d.speed_mts)
            {
                j["speed_mts"] = d.speed_mts->value;
            }
            if(d.manufacturer)
            {
                j["manufacturer"] = d.manufacturer->value;
            }
            if(d.part_number)
            {
                j["part_number"] = *d.part_number;
            }
            if(d.rank)
            {
                j["rank"] = *d.rank;
            }
            if(d.total_width)
            {
                j["total_width"] = *d.total_width;
            }
            if(d.data_width)
            {
                j["data_width"] = *d.data_width;
            }
            if(d.form_factor)
            {
                j["form_factor"] = *d.form_factor;
            }
            if(!d.memory_type.empty())
            {
                j["memory_type"] = d.memory_type;
            }
            if(!d.serial.empty())
            {
                j["serial"] = d.serial;
            }
            if(!d.asset_tag.empty())
            {
                j["asset_tag"] = d.asset_tag;
            }
            if(!d.type_detail.empty())
            {
                j["type_detail"] = d.type_detail;
            }
            if(!d.edac_mode.empty())
            {
                j["edac_mode"] = d.edac_mode;
            }
            if(!d.device_width.empty())
            {
                j["device_width"] = d.device_width;
            }
            if(d.configured_speed_mts)
            {
                j["configured_speed_mts"] = d.configured_speed_mts->value;
            }
            if(d.configured_voltage_mv)
            {
                j["configured_voltage_mv"] = d.configured_voltage_mv->value;
            }
            if(d.controller_index)
            {
                j["controller_index"] = *d.controller_index;
            }
            if(d.numa_node)
            {
                j["numa_node"] = d.numa_node->value();
            }
            if(d.edac_device_index)
                j["edac_device_index"] = d.edac_device_index->value();
            if(d.edac_association != DimmEdacAssociation::Unknown)
                j["edac_association"] = static_cast<std::uint32_t>(d.edac_association);
            return j;
        }

        [[nodiscard]] DimmInfo dimm_info_from_json(const json &j)
        {
            DimmInfo d;
            j.at("locator").get_to(d.locator);
            j.at("bank_locator").get_to(d.bank_locator);
            d.size = MemorySize{uint64_from_json(j.at("size"), "size")};
            d.present = j.at("present").get<bool>();
            if(j.contains("speed_mts"))
            {
                d.speed_mts = TransferRate{uint64_from_json(j.at("speed_mts"), "speed_mts")};
            }
            if(j.contains("manufacturer"))
            {
                d.manufacturer = Vendor{j.at("manufacturer").get<std::string>()};
            }
            if(j.contains("part_number"))
            {
                d.part_number = j.at("part_number").get<std::string>();
            }
            if(j.contains("rank"))
            {
                d.rank = uint32_from_json(j.at("rank"), "rank");
            }
            if(j.contains("total_width"))
            {
                d.total_width = uint32_from_json(j.at("total_width"), "total_width");
            }
            if(j.contains("data_width"))
            {
                d.data_width = uint32_from_json(j.at("data_width"), "data_width");
            }
            if(j.contains("form_factor"))
            {
                d.form_factor = j.at("form_factor").get<std::string>();
            }
            d.memory_type = j.value("memory_type", std::string{});
            d.serial = j.value("serial", std::string{});
            d.asset_tag = j.value("asset_tag", std::string{});
            d.type_detail = j.value("type_detail", std::string{});
            d.edac_mode = j.value("edac_mode", std::string{});
            d.device_width = j.value("device_width", std::string{});
            if(j.contains("configured_speed_mts"))
            {
                d.configured_speed_mts =
                    TransferRate{uint64_from_json(j.at("configured_speed_mts"), "configured_speed_mts")};
            }
            if(j.contains("configured_voltage_mv"))
            {
                d.configured_voltage_mv =
                    Millivolts{uint64_from_json(j.at("configured_voltage_mv"), "configured_voltage_mv")};
            }
            if(j.contains("controller_index"))
            {
                d.controller_index = uint32_from_json(j.at("controller_index"), "controller_index");
            }
            if(j.contains("numa_node"))
            {
                d.numa_node = NumaNodeId{uint32_from_json(j.at("numa_node"), "numa_node")};
            }
            if(j.contains("edac_device_index"))
                d.edac_device_index = EdacMemoryDeviceIndex{static_cast<std::uint32_t>(detail::checked_unsigned(
                    j.at("edac_device_index"), std::numeric_limits<std::uint32_t>::max(), "EDAC device index"))};
            if(j.contains("edac_association"))
                d.edac_association = static_cast<DimmEdacAssociation>(detail::checked_unsigned(
                    j.at("edac_association"), static_cast<std::uint32_t>(DimmEdacAssociation::LabelAndCapacity),
                    "EDAC association"));
            return d;
        }

    } // namespace

    [[nodiscard]] json memory_to_json(const Memory &m)
    {
        json j = {{"total_memory", m.total_memory.value}};
        if(m.available_memory)
        {
            j["available_memory"] = m.available_memory->value;
        }
        if(!m.memory_type.empty())
        {
            j["memory_type"] = m.memory_type;
        }
        if(m.configured_speed_mts)
        {
            j["configured_speed_mts"] = m.configured_speed_mts->value;
        }
        json arr = json::array();
        for(const auto &nm : m.numa_memory)
        {
            arr.push_back(numa_memory_to_json(nm));
        }
        j["numa_memory"] = std::move(arr);
        json dimms = json::array();
        for(const auto &d : m.dimms)
        {
            dimms.push_back(dimm_info_to_json(d));
        }
        j["dimms"] = std::move(dimms);
        if(m.dimm_count)
        {
            j["dimm_count"] = *m.dimm_count;
        }
        if(m.populated_dimms)
        {
            j["populated_dimms"] = *m.populated_dimms;
        }
        if(!m.dimm_inventory_source.empty())
        {
            j["dimm_inventory_source"] = m.dimm_inventory_source;
        }
        if(m.reported_slot_count)
        {
            j["reported_slot_count"] = *m.reported_slot_count;
        }
        if(m.reported_slots_complete)
        {
            j["reported_slots_complete"] = *m.reported_slots_complete;
        }
        if(!m.controllers.empty())
        {
            j["controllers"] = json::array();
            for(const auto &controller : m.controllers)
                j["controllers"].push_back(memory_controller_to_json(controller));
        }
        if(!m.reported_array_location.empty())
            j["reported_array_location"] = m.reported_array_location;
        if(!m.reported_array_error_correction.empty())
            j["reported_array_error_correction"] = m.reported_array_error_correction;
        if(m.reported_array_max_capacity)
            j["reported_array_max_capacity"] = m.reported_array_max_capacity->value;
        if(!m.edac_devices.empty())
            j["edac_devices"] = detail::edac_devices_to_json(m.edac_devices);
        return j;
    }

    [[nodiscard]] Memory memory_from_json(const json &j)
    {
        Memory m;
        m.total_memory = MemorySize{uint64_from_json(j.at("total_memory"), "total_memory")};
        if(j.contains("available_memory"))
        {
            m.available_memory = MemorySize{uint64_from_json(j.at("available_memory"), "available_memory")};
        }
        if(j.contains("memory_type"))
        {
            m.memory_type = j.at("memory_type").get<std::string>();
        }
        if(j.contains("configured_speed_mts"))
        {
            m.configured_speed_mts =
                TransferRate{uint64_from_json(j.at("configured_speed_mts"), "configured_speed_mts")};
        }
        if(j.contains("numa_memory"))
        {
            for(const auto &elem : j.at("numa_memory"))
            {
                m.numa_memory.push_back(numa_memory_from_json(elem));
            }
        }
        if(j.contains("dimms"))
        {
            for(const auto &elem : j.at("dimms"))
            {
                m.dimms.push_back(dimm_info_from_json(elem));
            }
        }
        if(j.contains("dimm_count"))
        {
            m.dimm_count = uint32_from_json(j.at("dimm_count"), "dimm_count");
        }
        if(j.contains("populated_dimms"))
        {
            m.populated_dimms = uint32_from_json(j.at("populated_dimms"), "populated_dimms");
        }
        m.dimm_inventory_source = j.value("dimm_inventory_source", std::string{});
        if(j.contains("reported_slot_count"))
        {
            m.reported_slot_count = uint32_from_json(j.at("reported_slot_count"), "reported_slot_count");
        }
        if(j.contains("reported_slots_complete"))
        {
            m.reported_slots_complete = j.at("reported_slots_complete").get<bool>();
        }
        if(j.contains("controllers"))
            for(const auto &controller : j.at("controllers"))
                m.controllers.push_back(memory_controller_from_json(controller));
        m.reported_array_location = j.value("reported_array_location", std::string{});
        m.reported_array_error_correction = j.value("reported_array_error_correction", std::string{});
        if(j.contains("reported_array_max_capacity"))
            m.reported_array_max_capacity =
                MemorySize{uint64_from_json(j.at("reported_array_max_capacity"), "reported_array_max_capacity")};
        if(j.contains("edac_devices"))
            m.edac_devices = detail::edac_devices_from_json(j.at("edac_devices"));
        return m;
    }

} // namespace sysal::detail
