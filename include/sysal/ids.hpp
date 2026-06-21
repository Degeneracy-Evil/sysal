#pragma once

#include "sysal/strong_id.hpp"

#include <cstdint>

namespace sysal
{

struct CpuPackageIdTag
{
};
struct CpuCoreIdTag
{
};
struct LogicalCpuIdTag
{
};
struct NumaNodeIdTag
{
};
struct AcceleratorIdTag
{
};
struct StorageIdTag
{
};
struct DriverIdTag
{
};
struct RdmaDeviceIdTag
{
};

using CpuPackageId = StrongId<std::uint32_t, CpuPackageIdTag>;
using CpuCoreId = StrongId<std::uint32_t, CpuCoreIdTag>;
using LogicalCpuId = StrongId<std::uint32_t, LogicalCpuIdTag>;
using NumaNodeId = StrongId<std::uint32_t, NumaNodeIdTag>;
using AcceleratorId = StrongId<std::uint32_t, AcceleratorIdTag>;
using StorageId = StrongId<std::uint32_t, StorageIdTag>;
using DriverId = StrongId<std::uint32_t, DriverIdTag>;
using RdmaDeviceId = StrongId<std::uint32_t, RdmaDeviceIdTag>;

} // namespace sysal
