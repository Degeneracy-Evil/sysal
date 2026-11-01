#pragma once

#include "sysal/model/inventory.hpp"

#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json enumeration_to_json(const InventoryEnumeration &enumeration);
    InventoryEnumeration enumeration_from_json(const nlohmann::json &json);
    nlohmann::json inventory_to_json(const InventoryReport &report);
    InventoryReport inventory_from_json(const nlohmann::json &json);
} // namespace sysal::detail
