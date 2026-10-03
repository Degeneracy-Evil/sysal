#pragma once

#include "sysal/model/accelerator.hpp"
#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json accelerators_to_json(const Accelerators &value);
    Accelerators accelerators_from_json(const nlohmann::json &json);
} // namespace sysal::detail
