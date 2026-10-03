#pragma once

#include "sysal/model/memory.hpp"

#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json edac_location_to_json(const EdacLocation &location);
    EdacLocation edac_location_from_json(const nlohmann::json &j);
    nlohmann::json edac_devices_to_json(const std::vector<EdacMemoryDevice> &devices);
    std::vector<EdacMemoryDevice> edac_devices_from_json(const nlohmann::json &j);
} // namespace sysal::detail
