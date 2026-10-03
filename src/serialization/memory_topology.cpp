#include "serialization/memory_topology.hpp"

#include "serialization/json_values.hpp"

#include <limits>

namespace sysal::detail
{
    namespace
    {
        using json = nlohmann::json;

        std::uint32_t index_value(const json &j, const char *field)
        {
            return static_cast<std::uint32_t>(checked_unsigned(j, std::numeric_limits<std::uint32_t>::max(), field));
        }
    } // namespace

    nlohmann::json edac_location_to_json(const EdacLocation &location)
    {
        json j{{"report", location.report}, {"coordinates", json::array()}};
        for(const auto &coordinate : location.coordinates)
            j["coordinates"].push_back(
                {{"layer", static_cast<std::uint32_t>(coordinate.layer)}, {"index", coordinate.index}});
        return j;
    }

    EdacLocation edac_location_from_json(const nlohmann::json &j)
    {
        EdacLocation location;
        location.report = j.value("report", std::string{});
        if(j.contains("coordinates"))
            for(const auto &coordinate : j.at("coordinates").get_ref<const json::array_t &>())
                location.coordinates.push_back(
                    {static_cast<EdacLocationLayer>(
                         checked_unsigned(coordinate.at("layer"),
                                          static_cast<std::uint32_t>(EdacLocationLayer::AllMemory), "EDAC layer")),
                     index_value(coordinate.at("index"), "EDAC coordinate")});
        return location;
    }

    nlohmann::json edac_devices_to_json(const std::vector<EdacMemoryDevice> &devices)
    {
        auto j = json::array();
        for(const auto &device : devices)
        {
            json value{{"controller_index", device.controller_index},
                       {"index", device.index.value()},
                       {"kind", static_cast<std::uint32_t>(device.kind)},
                       {"label", device.label},
                       {"location", edac_location_to_json(device.location)},
                       {"memory_type", device.memory_type},
                       {"edac_mode", device.edac_mode},
                       {"device_width", device.device_width}};
            if(device.size)
                value["size"] = device.size->value;
            if(device.numa_node)
                value["numa_node"] = device.numa_node->value();
            j.push_back(std::move(value));
        }
        return j;
    }

    std::vector<EdacMemoryDevice> edac_devices_from_json(const nlohmann::json &j)
    {
        std::vector<EdacMemoryDevice> devices;
        for(const auto &value : j.get_ref<const json::array_t &>())
        {
            EdacMemoryDevice device;
            device.controller_index = index_value(value.at("controller_index"), "EDAC controller");
            device.index = EdacMemoryDeviceIndex{index_value(value.at("index"), "EDAC device")};
            device.kind = static_cast<EdacMemoryDeviceKind>(checked_unsigned(
                value.at("kind"), static_cast<std::uint32_t>(EdacMemoryDeviceKind::Rank), "EDAC device kind"));
            device.label = value.value("label", std::string{});
            device.memory_type = value.value("memory_type", std::string{});
            device.edac_mode = value.value("edac_mode", std::string{});
            device.device_width = value.value("device_width", std::string{});
            if(value.contains("location"))
                device.location = edac_location_from_json(value.at("location"));
            if(value.contains("size"))
                device.size = MemorySize{
                    checked_unsigned(value.at("size"), std::numeric_limits<std::uint64_t>::max(), "EDAC size")};
            if(value.contains("numa_node"))
                device.numa_node = NumaNodeId{index_value(value.at("numa_node"), "EDAC NUMA node")};
            devices.push_back(std::move(device));
        }
        return devices;
    }
} // namespace sysal::detail
