#pragma once
#include "sysal/core/system.hpp"
namespace sysal::detail
{
    HardwareHealth hardware_health(const SystemInfo &info, Collect flags, const RawStore &raw);
}
