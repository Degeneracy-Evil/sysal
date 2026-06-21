#pragma once

#include "parsed_facts.hpp"
#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <optional>

namespace sysal::detail
{

std::optional<MemorySubsystem> parse_memory(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
