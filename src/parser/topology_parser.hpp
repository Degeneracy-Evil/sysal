/// @file topology_parser.hpp
/// @brief 拓扑信息解析器接口
/// @details 声明 parse_topology 函数，优先使用 hwloc 后端解析拓扑，
///          不可用时回退到 sysfs NUMA 与 PCI 设备亲和信息。

#pragma once

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/topology_info.hpp"

#include <optional>

namespace sysal::detail
{

/// @brief 从 RawStore 解析拓扑信息
/// @param raw 原始数据存储
/// @param diag 诊断信息容器，用于记录解析过程中的告警
/// @return 解析成功返回 TopologyInfo；无法获取任何拓扑数据时返回 std::nullopt
/// @details 解析策略：优先调用 hwloc 后端；若 hwloc 不可用或返回空，
///          则回退到 sysfs 解析 NUMA 节点关系及 PCI 设备的 NUMA 亲和。
std::optional<TopologyInfo> parse_topology(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
