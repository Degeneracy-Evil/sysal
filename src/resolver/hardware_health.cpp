#include "resolver/hardware_health.hpp"

#include <algorithm>

namespace sysal::detail
{
    namespace
    {
        template <typename Unit>
        void sensor_alerts(const std::vector<SensorReading<Unit>> &sensors, HardwareHealth &health,
                           HealthCoverage &coverage)
        {
            for(const auto &sensor : sensors)
            {
                const auto &identity = sensor.identity;
                coverage.usable_readings += (sensor.input.has_value() || sensor.average.has_value()) &&
                                            sensor.enabled.value_or(true) && !sensor.fault.value_or(false);
                coverage.alarm_reports += static_cast<std::uint32_t>(sensor.alarms.size()) + sensor.fault.has_value();
                const auto add = [&](SensorAlertKind kind, HealthSeverity severity, const std::string &origin)
                { health.sensor_alerts.push_back({identity.id, kind, severity, origin}); };
                if(sensor.fault.value_or(false))
                {
                    add(SensorAlertKind::Fault, HealthSeverity::Warning, identity.id.value + "_fault");
                    continue;
                }
                auto alarm = std::find_if(sensor.alarms.begin(), sensor.alarms.end(),
                                          [](const auto &item) {
                                              return item.active && (item.attribute.ends_with("crit_alarm") ||
                                                                     item.attribute.ends_with("emergency_alarm"));
                                          });
                if(alarm == sensor.alarms.end())
                    alarm = std::find_if(sensor.alarms.begin(), sensor.alarms.end(),
                                         [](const auto &item) { return item.active; });
                if(alarm != sensor.alarms.end())
                {
                    const auto slash = identity.origin.rfind('/');
                    const auto severity =
                        alarm->attribute.ends_with("crit_alarm") || alarm->attribute.ends_with("emergency_alarm")
                            ? HealthSeverity::Critical
                            : HealthSeverity::Warning;
                    add(SensorAlertKind::DriverAlarm, severity,
                        identity.origin.substr(0, slash + 1) + alarm->attribute);
                    continue;
                }
                if(!sensor.enabled.value_or(true) || !sensor.input || !sensor.unit_supported ||
                   (sensor.minimum && sensor.maximum && sensor.minimum->value > sensor.maximum->value))
                    continue;
                if(sensor.critical && sensor.input->value >= sensor.critical->value)
                    add(SensorAlertKind::CriticalLimit, HealthSeverity::Critical, identity.origin);
                else if(sensor.maximum && sensor.input->value > sensor.maximum->value)
                    add(SensorAlertKind::AboveMaximum, HealthSeverity::Warning, identity.origin);
                else if(sensor.minimum && sensor.input->value < sensor.minimum->value)
                    add(SensorAlertKind::BelowMinimum, HealthSeverity::Warning, identity.origin);
            }
        }
        HealthCoverageStatus status(bool requested, std::uint32_t count, bool failed)
        {
            return !requested   ? HealthCoverageStatus::NotRequested
                   : count == 0 ? HealthCoverageStatus::Unavailable
                   : failed     ? HealthCoverageStatus::Partial
                                : HealthCoverageStatus::Available;
        }
    } // namespace
    HardwareHealth hardware_health(const SystemInfo &info, Collect flags, const RawStore &raw)
    {
        HardwareHealth health;
        HealthCoverage sensors{"sensors"};
        if(has(flags, Collect::Sensors))
        {
            sensor_alerts(info.sensors.temperatures, health, sensors);
            sensor_alerts(info.sensors.fans, health, sensors);
            sensor_alerts(info.sensors.powers, health, sensors);
        }
        const auto sensor_failure = std::any_of(raw.records.begin(), raw.records.end(),
                                                [](const auto &record)
                                                {
                                                    return (record.source == RawSource::SysfsHwmon ||
                                                            record.source == RawSource::SysfsThermal) &&
                                                           record.status == CollectStatus::Failed;
                                                });
        sensors.status =
            status(has(flags, Collect::Sensors), sensors.usable_readings + sensors.alarm_reports, sensor_failure);
        health.coverage.push_back(sensors);
        HealthCoverage storage{"md"};
        bool incomplete_md = false;
        for(const auto &device : info.storage.devices)
        {
            incomplete_md |= device.layer == "md" && !device.raid_degraded;
            if(device.raid_degraded)
            {
                ++storage.usable_readings;
                if(*device.raid_degraded > 0)
                    health.storage_alerts.push_back({device.name, *device.raid_degraded, HealthSeverity::Warning,
                                                     "/sys/class/block/" + device.name.value + "/md/degraded"});
            }
        }
        storage.status = status(has(flags, Collect::Storage), storage.usable_readings, incomplete_md);
        health.coverage.push_back(storage);
        HealthCoverage memory{"edac"};
        bool incomplete_edac = false;
        for(const auto &controller : info.memory.controllers)
        {
            incomplete_edac |= !controller.corrected_errors || !controller.uncorrected_errors;
            memory.usable_readings +=
                controller.corrected_errors.has_value() || controller.uncorrected_errors.has_value();
            if(controller.corrected_errors.value_or(0) > 0 || controller.uncorrected_errors.value_or(0) > 0)
                health.memory_events.push_back(
                    {controller.index, controller.corrected_errors, controller.uncorrected_errors,
                     controller.uncorrected_errors.value_or(0) > 0 ? HealthSeverity::Warning
                                                                   : HealthSeverity::Information,
                     "/sys/devices/system/edac/mc/mc" + std::to_string(controller.index)});
        }
        memory.status = status(has(flags, Collect::Memory), memory.usable_readings, incomplete_edac);
        health.coverage.push_back(memory);
        return health;
    }
} // namespace sysal::detail
