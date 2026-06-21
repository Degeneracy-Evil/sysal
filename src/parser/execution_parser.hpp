#pragma once

#include "sysal/diagnostics.hpp"
#include "sysal/execution_context_info.hpp"
#include "sysal/raw_store.hpp"

namespace sysal::detail
{

ExecutionContextInfo parse_execution_context(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
