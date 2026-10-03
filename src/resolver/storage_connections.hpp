#pragma once

#include "sysal/model/pci.hpp"
#include "sysal/model/storage.hpp"

namespace sysal::detail
{
    void resolve_storage_connections(Storage &storage, const Pci &pci);
} // namespace sysal::detail
