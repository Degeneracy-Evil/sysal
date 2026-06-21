#pragma once

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/resource_info.hpp"

#include <optional>

namespace sysal::detail
{

std::optional<AcceleratorSubsystem> parse_accelerators(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
