#include "serialization/cpu_features.hpp"

namespace sysal::detail
{
    namespace
    {
        nlohmann::json ids_to_json(const std::vector<LogicalCpuId> &ids)
        {
            auto json = nlohmann::json::array();
            for(const auto id : ids)
                json.push_back(id.value());
            return json;
        }

        nlohmann::json names_to_json(const std::vector<CpuFeatureName> &names)
        {
            auto json = nlohmann::json::array();
            for(const auto &name : names)
                json.push_back(name.value);
            return json;
        }
    } // namespace

    nlohmann::json cpu_feature_summary_to_json(const CpuFeatureSummary &summary)
    {
        nlohmann::json json{{"logical_cpu_ids", ids_to_json(summary.logical_cpu_ids)},
                            {"complete_cpu_ids", ids_to_json(summary.complete_cpu_ids)},
                            {"observed_union", names_to_json(summary.observed_union)},
                            {"common", summary.common ? names_to_json(*summary.common) : nlohmann::json(nullptr)},
                            {"reports", nlohmann::json::array()}};
        for(const auto &report : summary.reports)
            json["reports"].push_back({{"name", report.name.value},
                                       {"reported_cpu_ids", ids_to_json(report.reported_cpu_ids)},
                                       {"not_reported_cpu_ids", ids_to_json(report.not_reported_cpu_ids)},
                                       {"unknown_cpu_ids", ids_to_json(report.unknown_cpu_ids)}});
        return json;
    }
} // namespace sysal::detail
