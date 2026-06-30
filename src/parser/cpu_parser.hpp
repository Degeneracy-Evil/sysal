#pragma once

#include "parsed_facts.hpp"
#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <optional>

namespace sysal::detail
{

/// @brief 解析 CPU 子系统信息
/// @details 从 RawStore 中的 /proc/cpuinfo 原始记录解析出 CPU 物理包、
///          物理核与逻辑 CPU 的层级结构，以及指令集扩展信息。
/// @param raw 原始数据存储，需包含 ProcCpuInfo 与 ProcUname 来源的记录
/// @param diag 诊断信息收集器，解析过程中的警告会写入此处
/// @return 解析成功返回 CpuSubsystem，无数据或解析失败返回 std::nullopt
std::optional<CpuSubsystem> parse_cpu(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
