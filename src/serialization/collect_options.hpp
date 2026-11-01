#pragma once
#include "sysal/core/collect_options.hpp"
#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json collection_request_to_json(const CollectionRequest &request);
    CollectionRequest collection_request_from_json(const nlohmann::json &json);
} // namespace sysal::detail
