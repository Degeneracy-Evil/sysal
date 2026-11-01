#pragma once
#include "sysal/model/storage.hpp"
#include <nlohmann/json.hpp>
namespace sysal::detail
{
    void filesystems_to_json(nlohmann::json &j, const Storage &storage);
    void filesystems_from_json(const nlohmann::json &j, Storage &storage);
} // namespace sysal::detail
