#pragma once

#include "sysal/collect_spec.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace sysal
{

struct SnapshotMeta
{
    std::chrono::system_clock::time_point collect_time;
    std::string sysal_version;
    std::chrono::milliseconds collect_duration{};
    CollectSpec requested_spec;
    std::vector<std::string> succeeded_collectors;
    std::vector<std::string> failed_collectors;
};

} // namespace sysal
