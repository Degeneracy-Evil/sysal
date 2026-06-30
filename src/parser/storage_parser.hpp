/// @file storage_parser.hpp
/// @brief 存储子系统解析器接口
/// @details 声明 parse_storage 函数，从 RawStore 中解析 sysfs 块设备信息，
///          构造 StorageSubsystem 事实。

#pragma once

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/resource_info.hpp"

#include <optional>

namespace sysal::detail
{

/// @brief 从 RawStore 解析存储子系统信息
/// @param raw 原始数据存储
/// @param diag 诊断信息容器，用于记录解析过程中的告警
/// @return 解析成功返回 StorageSubsystem；无块设备数据时返回 std::nullopt
/// @details 读取 sysfs 中 /sys/block/ 下的块设备条目，解析设备名、容量、
///          型号以及 PCI 地址，并按名称前缀分类设备类型（NVMe / SATA / Other）。
std::optional<StorageSubsystem> parse_storage(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
