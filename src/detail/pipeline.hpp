#pragma once

#include "sysal/collect_spec.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/system_snapshot.hpp"

#include <chrono>

namespace sysal::detail
{

SystemSnapshot run_pipeline(const RawStore& raw, const CollectSpec& spec,
                            std::chrono::system_clock::time_point start_time);

} // namespace sysal::detail
