#pragma once

#include "sysal/model/raw_store.hpp"
#include "sysal/model/rdma.hpp"

namespace sysal::detail
{
    RdmaInventory parse_rdma(const RawStore &raw);
}
