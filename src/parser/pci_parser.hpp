#pragma once

#include "parsed_facts.hpp"
#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <optional>

namespace sysal::detail
{

/// @brief 解析 PCI 子系统信息
/// @details 从 RawStore 中的 sysfs PCI 设备目录（/sys/bus/pci/devices/）解析出
///          每个 PCI 设备的地址、厂商 ID、设备 ID 与设备类别。
/// @param raw 原始数据存储，需包含 SysfsPci 来源的记录
/// @param diag 诊断信息收集器，解析过程中的警告会写入此处
/// @return 解析成功返回 PciSubsystem，无数据或无法解析设备地址返回 std::nullopt
std::optional<PciSubsystem> parse_pci(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
