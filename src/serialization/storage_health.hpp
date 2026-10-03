#pragma once
#include "sysal/model/storage_health.hpp"
#include <nlohmann/json.hpp>
namespace sysal::detail
{
    nlohmann::json storage_health_to_json(const StorageHealthReport &report);
    StorageHealthReport storage_health_from_json(const nlohmann::json &json);
} // namespace sysal::detail
