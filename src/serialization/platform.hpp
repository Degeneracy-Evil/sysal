#pragma once

#include "sysal/model/platform.hpp"
#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json platform_to_json(const Platform &value);
    Platform platform_from_json(const nlohmann::json &json);
} // namespace sysal::detail
