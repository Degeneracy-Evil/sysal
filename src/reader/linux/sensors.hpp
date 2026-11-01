#pragma once
#include "sysal/core/collect_options.hpp"
#include "sysal/model/raw_store.hpp"
namespace sysal::reader
{
    void read_sensors(RawStore &raw, SensorDevices policy = SensorDevices::All);
    void read_thermal_sensors(RawStore &raw, SensorDevices policy = SensorDevices::All);
} // namespace sysal::reader
