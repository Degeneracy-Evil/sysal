#include "serialization/hardware.hpp"
#include "sysal/core/error.hpp"
#include <limits>
#include <type_traits>
#include <utility>

namespace sysal::detail
{
    namespace
    {
        using json = nlohmann::json;
        template <typename Enum> Enum checked(const json &j, const char *key, Enum maximum)
        {
            const auto number = j.at(key).get<std::uint64_t>();
            if(number > static_cast<std::uint32_t>(maximum))
                throw SysalError(ErrorKind::DeserializationError, std::string("invalid ") + key);
            return static_cast<Enum>(number);
        }
        json identity_to_json(const SensorIdentity &identity)
        {
            json result{{"id", identity.id.value},   {"chip", identity.chip},
                        {"driver", identity.driver}, {"channel", identity.channel},
                        {"label", identity.label},   {"source", static_cast<unsigned>(identity.source)},
                        {"origin", identity.origin}, {"device_path", identity.device_path}};
            if(identity.pci_address)
            {
                const auto &address = *identity.pci_address;
                result["pci_address"] = {{"domain", address.domain},
                                         {"bus", address.bus},
                                         {"device", address.device},
                                         {"function", address.function}};
            }
            return result;
        }
        SensorIdentity identity_from_json(const json &j)
        {
            SensorIdentity identity;
            identity.id = SensorId{j.at("id").get<std::string>()};
            identity.chip = j.value("chip", std::string{});
            identity.driver = j.value("driver", std::string{});
            identity.channel = j.value("channel", std::string{});
            identity.label = j.value("label", std::string{});
            identity.origin = j.value("origin", std::string{});
            identity.device_path = j.value("device_path", std::string{});
            identity.source = checked(j, "source", RawSource::SysfsHwmon);
            if(j.contains("pci_address"))
            {
                const auto &a = j.at("pci_address");
                const auto domain = a.at("domain").get<std::uint64_t>();
                const auto bus = a.at("bus").get<std::uint64_t>();
                const auto device = a.at("device").get<std::uint64_t>();
                const auto function = a.at("function").get<std::uint64_t>();
                if(domain > 65535 || bus > 255 || device > 31 || function > 7)
                    throw SysalError(ErrorKind::DeserializationError, "invalid sensor PCI address");
                identity.pci_address =
                    PciAddress{static_cast<std::uint16_t>(domain), static_cast<std::uint8_t>(bus),
                               static_cast<std::uint8_t>(device), static_cast<std::uint8_t>(function)};
            }
            return identity;
        }
        template <typename Unit> json sensor_to_json(const SensorReading<Unit> &sensor)
        {
            json j{{"identity", identity_to_json(sensor.identity)},
                   {"unit_supported", sensor.unit_supported},
                   {"alarms", json::array()}};
            const std::pair<const char *, const std::optional<Unit> *> fields[] = {{"input", &sensor.input},
                                                                                   {"average", &sensor.average},
                                                                                   {"minimum", &sensor.minimum},
                                                                                   {"maximum", &sensor.maximum},
                                                                                   {"critical", &sensor.critical}};
            for(const auto &[key, field] : fields)
                if(*field)
                    j[key] = (*field)->value;
            if(sensor.enabled)
                j["enabled"] = *sensor.enabled;
            if(sensor.fault)
                j["fault"] = *sensor.fault;
            for(const auto &alarm : sensor.alarms)
                j["alarms"].push_back({{"attribute", alarm.attribute}, {"active", alarm.active}});
            return j;
        }
        template <typename Unit> Unit unit_from_json(const json &value)
        {
            using Number = decltype(Unit{}.value);
            if(!value.is_number_integer())
                throw SysalError(ErrorKind::DeserializationError, "sensor measurement must be an integer");
            if constexpr(std::is_signed_v<Number>)
            {
                if(value.is_number_unsigned() &&
                   value.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<Number>::max()))
                    throw SysalError(ErrorKind::DeserializationError, "sensor measurement out of range");
            }
            else if(!value.is_number_unsigned() && value.get<std::int64_t>() < 0)
                throw SysalError(ErrorKind::DeserializationError, "negative unsigned sensor measurement");
            return Unit{value.get<Number>()};
        }

        template <typename Unit> SensorReading<Unit> sensor_from_json(const json &j)
        {
            SensorReading<Unit> sensor;
            sensor.identity = identity_from_json(j.at("identity"));
            sensor.unit_supported = j.value("unit_supported", true);
            const std::pair<const char *, std::optional<Unit> *> fields[] = {{"input", &sensor.input},
                                                                             {"average", &sensor.average},
                                                                             {"minimum", &sensor.minimum},
                                                                             {"maximum", &sensor.maximum},
                                                                             {"critical", &sensor.critical}};
            for(const auto &[key, field] : fields)
                if(j.contains(key))
                    *field = unit_from_json<Unit>(j.at(key));
            if(j.contains("enabled"))
                sensor.enabled = j.at("enabled").get<bool>();
            if(j.contains("fault"))
                sensor.fault = j.at("fault").get<bool>();
            if(j.contains("alarms"))
                for(const auto &alarm : j.at("alarms"))
                    sensor.alarms.push_back({alarm.at("attribute").get<std::string>(), alarm.at("active").get<bool>()});
            return sensor;
        }
    } // namespace
    nlohmann::json sensors_to_json(const Sensors &sensors)
    {
        json j{{"temperatures", json::array()}, {"fans", json::array()}, {"powers", json::array()}};
        for(const auto &sensor : sensors.temperatures)
            j["temperatures"].push_back(sensor_to_json(sensor));
        for(const auto &sensor : sensors.fans)
            j["fans"].push_back(sensor_to_json(sensor));
        for(const auto &sensor : sensors.powers)
            j["powers"].push_back(sensor_to_json(sensor));
        return j;
    }
    Sensors sensors_from_json(const nlohmann::json &j)
    {
        Sensors sensors;
        if(j.contains("temperatures"))
            for(const auto &s : j.at("temperatures"))
                sensors.temperatures.push_back(sensor_from_json<SensorTemperature>(s));
        if(j.contains("fans"))
            for(const auto &s : j.at("fans"))
                sensors.fans.push_back(sensor_from_json<FanSpeed>(s));
        if(j.contains("powers"))
            for(const auto &s : j.at("powers"))
                sensors.powers.push_back(sensor_from_json<SensorPower>(s));
        return sensors;
    }
    nlohmann::json health_to_json(const HardwareHealth &health)
    {
        json j{{"sensor_alerts", json::array()},
               {"storage_alerts", json::array()},
               {"memory_events", json::array()},
               {"coverage", json::array()}};
        for(const auto &alert : health.sensor_alerts)
            j["sensor_alerts"].push_back({{"sensor", alert.sensor.value},
                                          {"kind", static_cast<unsigned>(alert.kind)},
                                          {"severity", static_cast<unsigned>(alert.severity)},
                                          {"origin", alert.origin}});
        for(const auto &alert : health.storage_alerts)
            j["storage_alerts"].push_back({{"device", alert.device.value},
                                           {"degraded_members", alert.degraded_members},
                                           {"severity", static_cast<unsigned>(alert.severity)},
                                           {"origin", alert.origin}});
        for(const auto &event : health.memory_events)
        {
            json e{{"controller_index", event.controller_index},
                   {"severity", static_cast<unsigned>(event.severity)},
                   {"origin", event.origin},
                   {"period", "since initialization/reset"}};
            if(event.corrected)
                e["corrected"] = *event.corrected;
            if(event.uncorrected)
                e["uncorrected"] = *event.uncorrected;
            j["memory_events"].push_back(e);
        }
        for(const auto &coverage : health.coverage)
            j["coverage"].push_back({{"domain", coverage.domain},
                                     {"status", static_cast<unsigned>(coverage.status)},
                                     {"usable_readings", coverage.usable_readings},
                                     {"alarm_reports", coverage.alarm_reports}});
        return j;
    }
    HardwareHealth health_from_json(const nlohmann::json &j)
    {
        HardwareHealth health;
        if(j.contains("sensor_alerts"))
            for(const auto &a : j.at("sensor_alerts"))
                health.sensor_alerts.push_back(
                    {SensorId{a.at("sensor").get<std::string>()}, checked(a, "kind", SensorAlertKind::AboveMaximum),
                     checked(a, "severity", HealthSeverity::Critical), a.at("origin").get<std::string>()});
        if(j.contains("storage_alerts"))
            for(const auto &a : j.at("storage_alerts"))
                health.storage_alerts.push_back(
                    {DeviceName{a.at("device").get<std::string>()}, a.at("degraded_members").get<std::uint32_t>(),
                     checked(a, "severity", HealthSeverity::Critical), a.at("origin").get<std::string>()});
        if(j.contains("memory_events"))
            for(const auto &e : j.at("memory_events"))
            {
                MemoryErrorEvent event;
                event.controller_index = e.at("controller_index").get<std::uint32_t>();
                if(e.contains("corrected"))
                    event.corrected = e.at("corrected").get<std::uint64_t>();
                if(e.contains("uncorrected"))
                    event.uncorrected = e.at("uncorrected").get<std::uint64_t>();
                event.severity = checked(e, "severity", HealthSeverity::Critical);
                event.origin = e.at("origin").get<std::string>();
                health.memory_events.push_back(event);
            }
        if(j.contains("coverage"))
            for(const auto &c : j.at("coverage"))
                health.coverage.push_back(
                    {c.at("domain").get<std::string>(), checked(c, "status", HealthCoverageStatus::Available),
                     c.at("usable_readings").get<std::uint32_t>(), c.at("alarm_reports").get<std::uint32_t>()});
        return health;
    }
} // namespace sysal::detail
