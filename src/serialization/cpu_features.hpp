#pragma once

#include "sysal/model/cpu_features.hpp"

#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json cpu_feature_summary_to_json(const CpuFeatureSummary &summary);
}
