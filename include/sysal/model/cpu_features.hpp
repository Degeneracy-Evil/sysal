#pragma once

#include "sysal/types/ids.hpp"
#include "sysal/types/value_types.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace sysal
{
    using CpuFeatureName = NamedString<struct CpuFeatureNameTag>;

    /// @brief Kernel reporting state; NotReported does not mean unsupported.
    enum class CpuFeatureState
    {
        Unknown,
        Reported,
        NotReported
    };

    /// @brief Normalize known Linux feature aliases, preserving all other identifiers.
    CpuFeatureName normalize_cpu_feature(std::string_view name);

    struct CpuFeatureReport
    {
        CpuFeatureName name;
        std::vector<LogicalCpuId> reported_cpu_ids;
        std::vector<LogicalCpuId> not_reported_cpu_ids;
        std::vector<LogicalCpuId> unknown_cpu_ids;
    };

    /// @brief Query result over observed logical CPU IDs, including offline CPUs.
    /// @details Coverage of this scope is separate from machine inventory completeness.
    ///          Duplicate IDs cannot establish feature evidence for that CPU.
    struct CpuFeatureSummary
    {
        std::vector<LogicalCpuId> logical_cpu_ids;
        std::vector<LogicalCpuId> complete_cpu_ids; ///< CPUs with a complete feature-field report.
        std::vector<CpuFeatureName> observed_union;
        /// @brief Complete intersection, or unknown when any scope member lacks a complete report.
        std::optional<std::vector<CpuFeatureName>> common;
        std::vector<CpuFeatureReport> reports; ///< Known capabilities plus all observed identifiers.
    };
} // namespace sysal
