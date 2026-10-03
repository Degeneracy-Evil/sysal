#pragma once

#include "sysal/types/enums.hpp"
#include "sysal/types/units.hpp"
#include "sysal/types/value_types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal
{
    struct SensorIdTag
    {
    };
    using SensorId = NamedString<SensorIdTag>;

    /// Signed millidegrees Celsius, separate from the legacy unsigned Temperature.
    struct SensorTemperature
    {
        std::int64_t value{};
        bool operator==(const SensorTemperature &) const = default;
    };
    struct FanSpeedTag
    {
    };
    struct SensorPowerTag
    {
    };
    using FanSpeed = ScalarUnit<FanSpeedTag>;       ///< RPM
    using SensorPower = ScalarUnit<SensorPowerTag>; ///< microwatts

    struct SensorIdentity
    {
        SensorId id;
        std::string chip;
        std::string driver;
        std::string channel;
        std::string label;
        RawSource source{};
        std::string origin;
        std::string device_path;
        std::optional<PciAddress> pci_address;
    };

    struct SensorAlarm
    {
        std::string attribute; ///< Kernel attribute; may represent a latched indication.
        bool active{};
    };

    /// Only values with the same physical unit share this representation.
    template <typename Unit> struct SensorReading
    {
        SensorIdentity identity;
        std::optional<Unit> input;
        std::optional<Unit> average;
        std::optional<Unit> minimum;
        std::optional<Unit> maximum;
        std::optional<Unit> critical;
        std::optional<bool> enabled;
        std::optional<bool> fault;
        std::vector<SensorAlarm> alarms;
        bool unit_supported{true};
    };
    using TemperatureSensor = SensorReading<SensorTemperature>;
    using FanSensor = SensorReading<FanSpeed>;
    using PowerSensor = SensorReading<SensorPower>;

    struct Sensors
    {
        std::vector<TemperatureSensor> temperatures;
        std::vector<FanSensor> fans;
        std::vector<PowerSensor> powers;
    };
} // namespace sysal
