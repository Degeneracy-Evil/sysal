#pragma once

#include "sysal/model/raw_store.hpp"
#include "sysal/model/storage.hpp"

namespace sysal::detail
{
    void apply_storage_connections(Storage &storage, const RawStore &raw);
} // namespace sysal::detail
