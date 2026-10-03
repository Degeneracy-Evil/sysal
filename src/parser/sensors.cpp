#include "parser/sensors.hpp"
#include "parser/parse_utils.hpp"

#include <charconv>
#include <map>
#include <set>
#include <string_view>
#include <type_traits>

namespace sysal::detail
{
    namespace
    {
        using Attributes = std::map<std::string, std::string>;
        std::string value(const Attributes &attributes, const std::string &key)
        {
            const auto it = attributes.find(key);
            return it == attributes.end() ? std::string{} : trim(it->second);
        }
        template <typename Unit> std::optional<Unit> measurement(const Attributes &attributes, const std::string &key)
        {
            const auto text = value(attributes, key);
            decltype(Unit{}.value) number{};
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
            if(text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
                return std::nullopt;
            return Unit{number};
        }
        std::optional<bool> flag(const Attributes &attributes, const std::string &key)
        {
            const auto text = value(attributes, key);
            return text == "0" ? std::optional{false} : text == "1" ? std::optional{true} : std::nullopt;
        }
        template <typename Unit>
        SensorReading<Unit> channel(const Attributes &attributes, const std::string &path, const std::string &name,
                                    RawSource source)
        {
            SensorReading<Unit> sensor;
            auto &identity = sensor.identity;
            identity.id = SensorId{path + "/" + name};
            identity.origin = path + "/" + name +
                              (attributes.contains(name + "_input")     ? "_input"
                               : attributes.contains(name + "_average") ? "_average"
                                                                        : "_input");
            identity.channel = name;
            identity.source = source;
            identity.chip = value(attributes, "name");
            identity.driver = value(attributes, "driver");
            identity.label = value(attributes, name + "_label");
            identity.device_path = value(attributes, "device_path");
            for(const auto &part : split(identity.device_path, '/'))
                if(auto address = parse_pci_address(part))
                    identity.pci_address = address;
            sensor.enabled = flag(attributes, name + "_enable");
            sensor.fault = flag(attributes, name + "_fault");
            // Type 4 alone does not specify whether the driver converted a thermistor voltage.
            // These drivers document converted Celsius; other thermistor channels stay unknown.
            sensor.unit_supported = !std::is_same_v<Unit, SensorTemperature> ||
                                    value(attributes, name + "_type") != "4" || identity.driver == "it87" ||
                                    identity.driver == "ntc-thermistor" || identity.driver == "ntc_thermistor";
            if(sensor.unit_supported)
            {
                sensor.input = measurement<Unit>(attributes, name + "_input");
                sensor.average = measurement<Unit>(attributes, name + "_average");
                sensor.minimum = measurement<Unit>(attributes, name + "_min");
                sensor.maximum = measurement<Unit>(attributes, name + "_max");
                sensor.critical = measurement<Unit>(attributes, name + "_crit");
            }
            for(const auto &[key, ignored] : attributes)
                if(key.starts_with(name + "_") && (key.ends_with("_alarm")))
                    if(auto active = flag(attributes, key))
                        sensor.alarms.push_back({key, *active});
            return sensor;
        }
    } // namespace
    Sensors parse_sensors(const RawStore &raw)
    {
        std::map<std::pair<RawSource, std::string>, Attributes> groups;
        for(const auto &record : raw.records)
            if((record.source == RawSource::SysfsHwmon || record.source == RawSource::SysfsThermal) &&
               record.status == CollectStatus::Success)
            {
                const auto slash = record.path_or_command.rfind('/');
                if(slash != std::string::npos)
                    groups[{record.source, record.path_or_command.substr(0, slash)}]
                          [record.path_or_command.substr(slash + 1)] = record.payload;
            }
        Sensors sensors;
        for(const auto &[key, attributes] : groups)
        {
            const auto &[source, path] = key;
            if(source == RawSource::SysfsThermal)
            {
                if(!attributes.contains("temp"))
                    continue;
                Attributes translated = attributes;
                translated["temp1_input"] = value(attributes, "temp");
                translated["temp1_label"] = value(attributes, "type");
                auto sensor = channel<SensorTemperature>(translated, path, "temp1", source);
                sensor.identity.origin = path + "/temp";
                sensor.identity.chip = value(attributes, "type");
                for(const auto &[field, trip_type] : attributes)
                    if(field.starts_with("trip_point_") && field.ends_with("_type") && trim(trip_type) == "critical")
                        if(auto limit =
                               measurement<SensorTemperature>(attributes, field.substr(0, field.size() - 4) + "temp"))
                            if(!sensor.critical || limit->value < sensor.critical->value)
                                sensor.critical = limit;
                sensors.temperatures.push_back(std::move(sensor));
                continue;
            }
            std::set<std::string> channels;
            for(const auto &[field, ignored] : attributes)
            {
                const auto separator = field.find('_');
                if(separator == std::string::npos)
                    continue;
                const auto name = field.substr(0, separator);
                const auto start = name.starts_with("temp")    ? 4U
                                   : name.starts_with("fan")   ? 3U
                                   : name.starts_with("power") ? 5U
                                                               : 0U;
                if(start && name.size() > start && name.find_first_not_of("0123456789", start) == std::string::npos)
                    channels.insert(name);
            }
            for(const auto &name : channels)
                if(name.starts_with("temp"))
                    sensors.temperatures.push_back(channel<SensorTemperature>(attributes, path, name, source));
                else if(name.starts_with("fan"))
                    sensors.fans.push_back(channel<FanSpeed>(attributes, path, name, source));
                else
                    sensors.powers.push_back(channel<SensorPower>(attributes, path, name, source));
        }
        return sensors;
    }
} // namespace sysal::detail
