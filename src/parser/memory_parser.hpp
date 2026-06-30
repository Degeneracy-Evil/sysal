#pragma once

#include "parsed_facts.hpp"
#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <optional>

namespace sysal::detail
{

/// @brief 解析内存子系统信息
/// @details 从 RawStore 中的 /proc/meminfo 原始记录解析出系统总内存与可用内存，
///          并从 sysfs NUMA 节点目录解析各 NUMA 节点的内存信息。
/// @param raw 原始数据存储，需包含 ProcMemInfo 与 SysfsNuma 来源的记录
/// @param diag 诊断信息收集器，解析过程中的警告会写入此处
/// @return 解析成功返回 MemorySubsystem，无数据或缺少 MemTotal 返回 std::nullopt
std::optional<MemorySubsystem> parse_memory(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
