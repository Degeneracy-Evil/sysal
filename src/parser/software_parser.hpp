#pragma once

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/software_stack_info.hpp"

#include <optional>

namespace sysal::detail
{

std::optional<SoftwareStackInfo> parse_software_stack(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
