/// @file ids.hpp
/// @brief 强类型标识符定义
/// @details 基于 StrongId 定义 sysal 中所有语义明确的标识符类型，
///          覆盖 CPU、NUMA、加速器、存储、驱动与 RDMA 设备。

#pragma once

#include "sysal/types/strong_id.hpp"

#include <cstdint>

namespace sysal
{

/// @brief CPU 物理封装（socket）ID 标签
struct CpuPackageIdTag
{
};
/// @brief CPU 物理核 ID 标签
struct CpuCoreIdTag
{
};
/// @brief 逻辑 CPU（硬件线程）ID 标签
struct LogicalCpuIdTag
{
};
/// @brief NUMA 节点 ID 标签
struct NumaNodeIdTag
{
};
/// @brief 加速器设备 ID 标签
struct AcceleratorIdTag
{
};
/// @brief 存储设备 ID 标签
struct StorageIdTag
{
};
/// @brief 驱动 ID 标签
struct DriverIdTag
{
};
/// @brief RDMA 设备 ID 标签
struct RdmaDeviceIdTag
{
};

using CpuPackageId = StrongId<std::uint32_t, CpuPackageIdTag>;   ///< CPU 封装 ID
using CpuCoreId = StrongId<std::uint32_t, CpuCoreIdTag>;         ///< CPU 物理核 ID
using LogicalCpuId = StrongId<std::uint32_t, LogicalCpuIdTag>;   ///< 逻辑 CPU ID
using NumaNodeId = StrongId<std::uint32_t, NumaNodeIdTag>;       ///< NUMA 节点 ID
using AcceleratorId = StrongId<std::uint32_t, AcceleratorIdTag>; ///< 加速器设备 ID
using StorageId = StrongId<std::uint32_t, StorageIdTag>;         ///< 存储设备 ID
using DriverId = StrongId<std::uint32_t, DriverIdTag>;           ///< 驱动 ID
using RdmaDeviceId = StrongId<std::uint32_t, RdmaDeviceIdTag>;   ///< RDMA 设备 ID

} // namespace sysal
