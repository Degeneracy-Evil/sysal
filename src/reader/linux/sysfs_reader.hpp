/// @file sysfs_reader.hpp
/// @brief sysfs 采集器接口
/// @details 声明 read_sysfs 函数，负责按 CollectSpec 从 /sys 文件系统
///          采集 CPU 拓扑、NUMA 节点、网络接口、PCI 设备、块设备等原始信息。

#pragma once

#include "sysal/collect_spec.hpp"
#include "sysal/raw_store.hpp"

namespace sysal::detail
{

/// @brief 采集 /sys 文件系统的原始数据
/// @param raw 输出的 RawStore，采集结果会追加到其中
/// @param spec 采集规格，决定哪些类别需要采集
void read_sysfs(RawStore& raw, const CollectSpec& spec);

} // namespace sysal::detail
