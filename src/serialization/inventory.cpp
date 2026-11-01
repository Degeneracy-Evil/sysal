#include "serialization/inventory.hpp"

#include "serialization/json_values.hpp"

namespace sysal::detail
{
    namespace
    {
        InventoryKind inventory_kind(const nlohmann::json &json)
        {
            return validate_enum(uint32_from_json(json.at("inventory"), "inventory"), InventoryKind::AcceleratorDevices,
                                 "inventory");
        }
    } // namespace

    nlohmann::json enumeration_to_json(const InventoryEnumeration &enumeration)
    {
        nlohmann::json json{{"inventory", static_cast<unsigned>(enumeration.inventory)},
                            {"source", static_cast<unsigned>(enumeration.source)},
                            {"origin", enumeration.origin},
                            {"entries", enumeration.entries},
                            {"status", static_cast<unsigned>(enumeration.status)}};
        if(enumeration.failure)
            json["failure"] = static_cast<unsigned>(*enumeration.failure);
        return json;
    }

    InventoryEnumeration enumeration_from_json(const nlohmann::json &json)
    {
        InventoryEnumeration enumeration;
        enumeration.inventory = inventory_kind(json);
        enumeration.source =
            validate_enum(uint32_from_json(json.at("source"), "source"), RawSource::FilesystemUsage, "source");
        enumeration.origin = json.at("origin").get<std::string>();
        enumeration.entries = json.at("entries").get<std::vector<std::string>>();
        enumeration.status =
            validate_enum(uint32_from_json(json.at("status"), "status"), CollectStatus::NotCollected, "status");
        if(json.contains("failure"))
            enumeration.failure =
                validate_enum(uint32_from_json(json.at("failure"), "failure"), ReadFailure::LowPower, "failure");
        return enumeration;
    }

    nlohmann::json inventory_to_json(const InventoryReport &report)
    {
        return {{"inventory", static_cast<unsigned>(report.inventory)},
                {"completeness", static_cast<unsigned>(report.completeness)},
                {"origin", report.origin}};
    }

    InventoryReport inventory_from_json(const nlohmann::json &json)
    {
        return {inventory_kind(json),
                validate_enum(uint32_from_json(json.at("completeness"), "completeness"),
                              InventoryCompleteness::Complete, "completeness"),
                json.at("origin").get<std::string>()};
    }
} // namespace sysal::detail
