#pragma once

#include "sysal/model/raw_store.hpp"

namespace sysal::reader
{
    /// Read the current cgroup and its mounted ancestors without leaving the mount root.
    void read_cgroup_limits(RawStore &raw);
} // namespace sysal::reader
