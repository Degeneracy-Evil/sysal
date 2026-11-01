#pragma once

#include "sysal/model/raw_store.hpp"
#include "sysal/model/storage.hpp"

#include <string_view>

namespace sysal::detail
{
    /// @brief Preserve historical output only; new classification never consumes these fields.
    void apply_storage_compatibility(StorageDevice &device, std::string_view sysfs_target);
    void apply_legacy_storage_mounts(Storage &storage, const RawStore &raw, std::vector<std::string> &warnings);
} // namespace sysal::detail
