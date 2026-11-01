#pragma once

#include "sysal/model/storage.hpp"

#include <string_view>

namespace sysal::detail
{
    /// @brief Preserve historical output only; new classification never consumes these fields.
    void apply_storage_compatibility(StorageDevice &device, std::string_view sysfs_target);
} // namespace sysal::detail
