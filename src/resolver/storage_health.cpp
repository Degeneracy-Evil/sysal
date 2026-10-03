#include "resolver/storage_health.hpp"
#include <algorithm>

namespace sysal::detail
{
    namespace
    {
        bool positive(const std::optional<StorageCounter> &counter)
        {
            return counter && counter->decimal != "0";
        }
        bool evidence(const StorageHealthReport &report)
        {
            if(report.passed.has_value() || !report.ata_attributes.empty())
                return true;
            if(report.nvme)
            {
                const auto &h = *report.nvme;
                if(h.critical_warning || h.available_spare || h.percentage_used || h.media_errors || h.power_on_hours)
                    return true;
            }
            return report.scsi &&
                   (report.scsi->read_uncorrected || report.scsi->write_uncorrected || report.scsi->verify_uncorrected);
        }
    } // namespace
    void storage_findings(HardwareHealth &health, const Storage &storage, Collect flags, const RawStore &raw)
    {
        HealthCoverage coverage{"storage_health"};
        bool incomplete = std::any_of(
            raw.records.begin(), raw.records.end(), [](const auto &record)
            { return record.source == RawSource::StorageHealthSysfs && record.status != CollectStatus::Success; });
        for(const auto &report : storage.health)
        {
            incomplete |= report.status != CollectStatus::Success || !evidence(report);
            coverage.usable_readings += evidence(report);
            coverage.alarm_reports += report.passed.has_value();
            const auto add = [&](DriveFindingKind kind, HealthSeverity severity, const std::string &field,
                                 std::optional<std::uint8_t> attribute = {}) {
                health.drive_findings.push_back(
                    {report.target, kind, severity, report.origin + "#" + field, attribute});
            };
            if(!report.passed.value_or(true) && (!report.nvme || report.nvme->critical_warning.value_or(0) == 0))
                add(DriveFindingKind::SmartFailed, HealthSeverity::Critical, "smart_status/passed");
            if(report.nvme)
            {
                const auto &nvme = *report.nvme;
                coverage.alarm_reports += nvme.critical_warning.has_value();
                const auto prefix = report.source == RawSource::Smartctl ? "nvme_smart_health_information_log/" : "";
                if(nvme.critical_warning.value_or(0) > 0)
                    add(DriveFindingKind::NvmeWarning, HealthSeverity::Critical,
                        std::string(prefix) + "critical_warning");
                if(nvme.available_spare && nvme.spare_threshold && *nvme.available_spare < *nvme.spare_threshold &&
                   !(nvme.critical_warning.value_or(0) & 1))
                    add(DriveFindingKind::SpareLow, HealthSeverity::Warning, std::string(prefix) + "available_spare");
                if(nvme.percentage_used.value_or(0) >= 100)
                    add(DriveFindingKind::EnduranceEstimate, HealthSeverity::Warning,
                        std::string(prefix) +
                            (report.source == RawSource::Smartctl ? "percentage_used" : "percent_used"));
                if(positive(nvme.media_errors))
                    add(DriveFindingKind::NvmeMediaHistory, HealthSeverity::Information,
                        std::string(prefix) + "media_errors");
            }
            for(const auto &attribute : report.ata_attributes)
            {
                if(attribute.when_failed == "now")
                    add(DriveFindingKind::AtaCurrent, HealthSeverity::Warning, "ata_smart_attributes", attribute.id);
                else if(attribute.when_failed == "past")
                    add(DriveFindingKind::AtaHistorical, HealthSeverity::Information, "ata_smart_attributes",
                        attribute.id);
            }
            if(report.scsi)
            {
                const auto &scsi = *report.scsi;
                const std::pair<const char *, const std::optional<StorageCounter> *> counters[] = {
                    {"read", &scsi.read_uncorrected},
                    {"write", &scsi.write_uncorrected},
                    {"verify", &scsi.verify_uncorrected}};
                for(const auto &[operation, counter] : counters)
                    if(positive(*counter))
                        add(DriveFindingKind::ScsiHistory, HealthSeverity::Information,
                            "scsi_error_counter_log/" + std::string(operation) + "/total_uncorrected_errors");
            }
        }
        coverage.status = !has(flags, Collect::StorageHealth) ? HealthCoverageStatus::NotRequested
                          : coverage.usable_readings == 0     ? HealthCoverageStatus::Unavailable
                          : incomplete                        ? HealthCoverageStatus::Partial
                                                              : HealthCoverageStatus::Available;
        health.coverage.push_back(coverage);
    }
} // namespace sysal::detail
