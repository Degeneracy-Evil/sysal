#pragma once

#include "sysal/model/cpu.hpp"
#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json cpu_to_json(const Cpu &value);
    Cpu cpu_from_json(const nlohmann::json &json);
} // namespace sysal::detail
