#pragma once

#include "parsed_facts.hpp"
#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <optional>

namespace sysal::detail
{

/// @brief 解析网络子系统信息
/// @details 从 RawStore 中的 sysfs 网络接口目录（/sys/class/net/）解析出每个接口
///          的状态、MAC 地址、速率、关联的 PCI 地址等属性，并通过 getifaddrs 收集
///          接口的 IP 地址列表。
/// @param raw 原始数据存储，需包含 SysfsNet 来源的记录
/// @param diag 诊断信息收集器，解析过程中的警告会写入此处
/// @return 解析成功返回 NetworkSubsystem，无数据返回 std::nullopt
std::optional<NetworkSubsystem> parse_network(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
