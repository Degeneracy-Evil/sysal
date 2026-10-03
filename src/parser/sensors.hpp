#pragma once
#include "sysal/model/raw_store.hpp"
#include "sysal/model/sensors.hpp"
namespace sysal::detail
{
    Sensors parse_sensors(const RawStore &raw);
}
