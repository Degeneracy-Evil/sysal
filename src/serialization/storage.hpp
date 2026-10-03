#pragma once

#include "sysal/model/storage.hpp"
#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json storage_to_json(const Storage &value);
    Storage storage_from_json(const nlohmann::json &json);
} // namespace sysal::detail
