#pragma once

#include "sysal/model/network.hpp"
#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json network_to_json(const Network &value);
    Network network_from_json(const nlohmann::json &json);
} // namespace sysal::detail
