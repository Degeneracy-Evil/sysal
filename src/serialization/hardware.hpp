#pragma once
#include "sysal/model/hardware_health.hpp"
#include "sysal/model/sensors.hpp"
#include <nlohmann/json.hpp>
namespace sysal::detail
{
    nlohmann::json sensors_to_json(const Sensors &sensors);
    Sensors sensors_from_json(const nlohmann::json &json);
    nlohmann::json health_to_json(const HardwareHealth &health);
    HardwareHealth health_from_json(const nlohmann::json &json);
} // namespace sysal::detail
