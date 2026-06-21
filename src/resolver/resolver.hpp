#pragma once

#include "parser/parsed_facts.hpp"
#include "sysal/collect_spec.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/system_snapshot.hpp"

#include <chrono>
#include <optional>
#include <utility>

namespace sysal::detail
{

SystemSnapshot resolve(ParsedFacts&& facts, const CollectSpec& spec,
                       std::chrono::system_clock::time_point start_time,
                       const std::optional<RawStore>& raw);

} // namespace sysal::detail
