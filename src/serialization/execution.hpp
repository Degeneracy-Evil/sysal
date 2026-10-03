#pragma once

#include "sysal/model/execution.hpp"
#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json execution_to_json(const ExecutionContext &value);
    ExecutionContext execution_from_json(const nlohmann::json &json);
} // namespace sysal::detail
