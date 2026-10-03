#pragma once

#include "sysal/model/pci.hpp"
#include "sysal/model/raw_store.hpp"
#include "sysal/model/storage.hpp"

namespace sysal::detail
{
    void apply_storage_connections(Storage &storage, const RawStore &raw);
    void resolve_storage_connections(Storage &storage, const Pci &pci);
} // namespace sysal::detail
