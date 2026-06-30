/// @file accelerator_parser.hpp
/// @brief 加速器子系统解析器接口
/// @details 声明 parse_accelerators 函数，从 nvidia-smi CSV 输出中
///          解析 GPU 加速器设备列表，构造 AcceleratorSubsystem 事实。

#pragma once

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/resource_info.hpp"

#include <optional>

namespace sysal::detail
{

/// @brief 从 RawStore 解析加速器子系统信息
/// @param raw 原始数据存储
/// @param diag 诊断信息容器，用于记录解析过程中的告警
/// @return 解析出至少一个设备时返回 AcceleratorSubsystem；否则返回 std::nullopt
/// @details 当前仅支持 NVIDIA GPU，数据来自 nvidia-smi CSV 输出。
///          CSV 列依次为：index, name, memory, pci_address, driver_version。
std::optional<AcceleratorSubsystem> parse_accelerators(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
