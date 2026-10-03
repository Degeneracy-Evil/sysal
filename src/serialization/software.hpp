#pragma once

#include "sysal/model/software.hpp"
#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json software_to_json(const SoftwareStack &value);
    SoftwareStack software_from_json(const nlohmann::json &json);
} // namespace sysal::detail
