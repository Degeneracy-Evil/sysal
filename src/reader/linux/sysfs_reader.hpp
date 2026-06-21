#pragma once

#include "sysal/collect_spec.hpp"
#include "sysal/raw_store.hpp"

namespace sysal::detail
{

void read_sysfs(RawStore& raw, const CollectSpec& spec);

} // namespace sysal::detail
