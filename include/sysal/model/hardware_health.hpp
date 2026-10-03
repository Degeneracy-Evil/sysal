#pragma once

#include "sysal/model/sensors.hpp"
#include "sysal/types/value_types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal
{
    enum class HealthSeverity
    {
        Information,
        Warning,
        Critical
    };
    enum class SensorAlertKind
    {
        DriverAlarm,
        Fault,
        CriticalLimit,
        BelowMinimum,
        AboveMaximum
    };
    enum class HealthCoverageStatus
    {
        NotRequested,
        Unavailable,
        Partial,
        Available
    };

    struct SensorAlert
    {
        SensorId sensor;
        SensorAlertKind kind{};
        HealthSeverity severity{};
        std::string origin; ///< Exact flag or measurement path; value/limit are in Sensors.
    };
    struct StorageAlert
    {
        DeviceName device;
        std::uint32_t degraded_members{};
        HealthSeverity severity{HealthSeverity::Warning};
        std::string origin;
    };
    struct MemoryErrorEvent
    {
        std::uint32_t controller_index{};
        std::optional<std::uint64_t> corrected;
        std::optional<std::uint64_t> uncorrected;
        HealthSeverity severity{};
        std::string origin;
        // Counts since driver initialization/reset, never a current error rate.
    };
    struct HealthCoverage
    {
        std::string domain;
        HealthCoverageStatus status{HealthCoverageStatus::NotRequested};
        std::uint32_t usable_readings{};
        std::uint32_t alarm_reports{};
    };
    enum class DriveFindingKind
    {
        SmartFailed,
        NvmeWarning,
        SpareLow,
        EnduranceEstimate,
        AtaCurrent,
        AtaHistorical,
        NvmeMediaHistory,
        ScsiHistory
    };
    struct DriveFinding
    {
        DeviceName target;
        DriveFindingKind kind{};
        HealthSeverity severity{};
        std::string origin;
        std::optional<std::uint8_t> attribute_id;
    };
    struct HardwareHealth
    {
        std::vector<SensorAlert> sensor_alerts;
        std::vector<StorageAlert> storage_alerts;
        std::vector<MemoryErrorEvent> memory_events;
        std::vector<HealthCoverage> coverage;
        std::vector<DriveFinding> drive_findings;
    };
} // namespace sysal
