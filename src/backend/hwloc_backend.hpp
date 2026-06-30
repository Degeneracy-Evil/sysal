/// @file hwloc_backend.hpp
/// @brief hwloc 拓扑后端接口声明
/// @details 声明基于 hwloc 库解析系统拓扑（NUMA 关系与设备位置）的入口函数，
///          当项目未启用 hwloc 时该函数恒返回 std::nullopt。

#pragma once

#include "sysal/diagnostics.hpp"
#include "sysal/topology_info.hpp"

#include <optional>

namespace sysal::detail
{

/// @brief 通过 hwloc 解析系统拓扑信息
/// @details 在启用 SYSAL_HAVE_HWLOC 时，调用 hwloc API 采集 NUMA 节点本地内存
///          以及 PCI 设备到最近 NUMA 节点的位置关系；未启用 hwloc 时直接返回
///          std::nullopt。解析过程中的失败会以警告形式写入 diag。
/// @param diag 诊断信息收集器，失败时写入警告
/// @return 成功且非空返回 TopologyInfo，否则返回 std::nullopt
std::optional<TopologyInfo> parse_topology_hwloc(Diagnostics& diag);

} // namespace sysal::detail
