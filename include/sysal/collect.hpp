#pragma once

#include "sysal/collect_spec.hpp"
#include "sysal/error.hpp"
#include "sysal/expected.hpp"
#include "sysal/system_snapshot.hpp"

namespace sysal
{

Expected<SystemSnapshot, SysalError> collect(const CollectSpec& spec);
SystemSnapshot collect_or_throw(const CollectSpec& spec);

} // namespace sysal
