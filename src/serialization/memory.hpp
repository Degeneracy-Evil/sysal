#pragma once

#include "sysal/model/memory.hpp"
#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json memory_to_json(const Memory &value);
    Memory memory_from_json(const nlohmann::json &json);
} // namespace sysal::detail
