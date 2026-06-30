/// @file procfs_reader.hpp
/// @brief procfs 采集器接口
/// @details 声明 read_procfs 函数，负责按 CollectSpec 从 /proc 文件系统
///          及相关命令采集原始系统信息（cpuinfo、meminfo、net/dev、uname 等）。

#pragma once

#include "sysal/collect_spec.hpp"
#include "sysal/raw_store.hpp"

namespace sysal::detail
{

/// @brief 采集 /proc 及相关命令的原始数据
/// @param raw 输出的 RawStore，采集结果会追加到其中
/// @param spec 采集规格，决定哪些类别需要采集
void read_procfs(RawStore& raw, const CollectSpec& spec);

} // namespace sysal::detail
