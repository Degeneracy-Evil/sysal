#pragma once

#include "sysal/model/execution.hpp"
#include "sysal/model/raw_store.hpp"

namespace sysal::detail
{
    /// Parse already captured hierarchy limits; no filesystem calls are made here.
    void parse_cgroup_limits(const RawStore &raw, ExecutionContext &context, std::vector<std::string> &warnings);
} // namespace sysal::detail
