#pragma once

#include "sysal/model/storage_classification.hpp"

#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json storage_classification_to_json(const StorageClassification &classification);
    StorageClassification storage_classification_from_json(const nlohmann::json &json);
} // namespace sysal::detail
