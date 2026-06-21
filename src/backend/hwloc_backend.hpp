#pragma once

#include "sysal/diagnostics.hpp"
#include "sysal/topology_info.hpp"

#include <optional>

namespace sysal::detail
{

std::optional<TopologyInfo> parse_topology_hwloc(Diagnostics& diag);

} // namespace sysal::detail
