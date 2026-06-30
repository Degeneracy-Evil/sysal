/// @file software_parser.hpp
/// @brief 软件栈解析器接口
/// @details 声明 parse_software_stack 函数，从 nvidia-smi、nvcc、/proc/driver/nvidia
///          等输出中提取驱动、CUDA 运行时及设备数量等软件栈信息。

#pragma once

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/software_stack_info.hpp"

#include <optional>

namespace sysal::detail
{

/// @brief 从 RawStore 解析软件栈信息
/// @param raw 原始数据存储
/// @param diag 诊断信息容器，用于记录解析过程中的告警
/// @return 解析出驱动或 CUDA 版本时返回 SoftwareStackInfo；否则返回 std::nullopt
/// @details 主要来源为 RawSource::NvidiaSmi，覆盖三类命令：
///          - /proc/driver/nvidia/version：NVRM 内核驱动版本；
///          - nvcc --version：CUDA Toolkit 版本；
///          - nvidia-smi ...：设备数量及驱动版本（CSV 最后一列）。
std::optional<SoftwareStackInfo> parse_software_stack(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
