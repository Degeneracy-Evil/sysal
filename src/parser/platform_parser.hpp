#pragma once

#include "parsed_facts.hpp"
#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <optional>

namespace sysal::detail
{

/// @brief 解析平台信息
/// @details 从 RawStore 中的 /etc/os-release、/proc/version、uname 输出等原始记录
///          解析出操作系统、内核、主机名与架构信息，并通过 gethostname 获取主机名。
/// @param raw 原始数据存储，需包含 ProcUname 与 ProcVersion 来源的记录
/// @param diag 诊断信息收集器，解析过程中的警告会写入此处
/// @return 始终返回 PlatformInfo（部分字段可能为空），无法确定架构时写入警告
std::optional<PlatformInfo> parse_platform(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
