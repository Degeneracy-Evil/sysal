#pragma once

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/topology_info.hpp"

#include <optional>

namespace sysal::detail
{

std::optional<TopologyInfo> parse_topology(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
