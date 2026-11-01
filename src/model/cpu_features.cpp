#include "sysal/model/cpu.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace sysal
{
    namespace
    {
        constexpr std::array<std::pair<std::string_view, std::string_view>, 5> aliases{{
            {"pni", "sse3"},
            {"sha_ni", "sha"},
            {"cx16", "cmpxchg16b"},
            {"lahf_lm", "lahf_sahf"},
            {"avx512_vnni", "avx512vnni"},
        }};
        // Capability vocabulary, independent of any application's display selection.
        constexpr std::array<std::string_view, 23> known_features{
            "sse",       "sse2",       "sse3",     "ssse3",    "sse4_1",   "sse4_2",     "avx",      "avx2",
            "avx512f",   "avx512cd",   "avx512bw", "avx512dq", "avx512vl", "aes",        "fma",      "f16c",
            "pclmulqdq", "avx512vnni", "sha",      "bmi1",     "bmi2",     "cmpxchg16b", "lahf_sahf"};

        struct FeatureEvidence
        {
            bool complete{};
            bool duplicate{};
            std::set<std::string> features;
        };
        using FeatureIndex = std::map<std::uint32_t, FeatureEvidence>;

        FeatureIndex feature_index(const Cpu &cpu)
        {
            FeatureIndex index;
            for(const auto &logical : cpu.logical_cpus)
            {
                auto [position, inserted] = index.try_emplace(logical.id.value());
                auto &evidence = position->second;
                if(!inserted)
                {
                    evidence.complete = false;
                    evidence.duplicate = true;
                    evidence.features.clear();
                    continue;
                }
                evidence.complete = logical.identification.features_reported;
                for(const auto &feature : logical.identification.features)
                    if(!feature.empty())
                        evidence.features.insert(normalize_cpu_feature(feature).value);
            }
            return index;
        }

        CpuFeatureReport feature_report(const FeatureIndex &index, const CpuFeatureName &name)
        {
            CpuFeatureReport report{normalize_cpu_feature(name.value), {}, {}, {}};
            for(const auto &[number, evidence] : index)
            {
                const auto id = LogicalCpuId{number};
                const bool usable = !report.name.value.empty() && !evidence.duplicate;
                if(usable && evidence.features.contains(report.name.value))
                    report.reported_cpu_ids.push_back(id);
                else if(usable && evidence.complete)
                    report.not_reported_cpu_ids.push_back(id);
                else
                    report.unknown_cpu_ids.push_back(id);
            }
            return report;
        }
    } // namespace

    CpuFeatureName normalize_cpu_feature(std::string_view name)
    {
        for(const auto &[alias, canonical] : aliases)
            if(name == alias)
                return {std::string{canonical}};
        return {std::string{name}};
    }

    CpuFeatureState CpuIdentification::feature(const CpuFeatureName &name) const
    {
        const auto canonical = normalize_cpu_feature(name.value);
        if(canonical.value.empty())
            return CpuFeatureState::Unknown;
        if(std::ranges::any_of(features, [&](const auto &item) { return normalize_cpu_feature(item) == canonical; }))
            return CpuFeatureState::Reported;
        return features_reported ? CpuFeatureState::NotReported : CpuFeatureState::Unknown;
    }

    CpuFeatureReport Cpu::feature(const CpuFeatureName &name) const
    {
        return feature_report(feature_index(*this), name);
    }

    CpuFeatureSummary Cpu::feature_summary() const
    {
        const auto index = feature_index(*this);
        CpuFeatureSummary summary;
        std::set<std::string> observed;
        for(const auto &[number, evidence] : index)
        {
            summary.logical_cpu_ids.emplace_back(number);
            if(evidence.complete)
                summary.complete_cpu_ids.emplace_back(number);
            observed.insert(evidence.features.begin(), evidence.features.end());
        }
        for(const auto &name : observed)
            summary.observed_union.push_back({name});
        if(!index.empty() && summary.complete_cpu_ids.size() == index.size())
            summary.common.emplace();
        auto vocabulary = observed;
        for(const auto name : known_features)
            vocabulary.insert(std::string{name});
        for(const auto &name : vocabulary)
        {
            auto report = feature_report(index, CpuFeatureName{name});
            if(summary.common && report.reported_cpu_ids.size() == index.size())
                summary.common->push_back(report.name);
            summary.reports.push_back(std::move(report));
        }
        return summary;
    }
} // namespace sysal
