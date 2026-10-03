#pragma once

#include "sysal/model/raw_store.hpp"

namespace sysal::reader
{
    void read_rdma_sysfs(RawStore &raw);
}
