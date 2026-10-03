#pragma once

#include "sysal/model/storage.hpp"

#include <nlohmann/json.hpp>

namespace sysal::detail
{
    void storage_connections_to_json(nlohmann::json &j, const Storage &storage);
    void storage_connections_from_json(const nlohmann::json &j, Storage &storage);
    void storage_device_connections_to_json(nlohmann::json &j, const StorageDevice &device);
    void storage_device_connections_from_json(const nlohmann::json &j, StorageDevice &device);
} // namespace sysal::detail
