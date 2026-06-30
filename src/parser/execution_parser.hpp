/// @file execution_parser.hpp
/// @brief 执行上下文解析器接口
/// @details 声明 parse_execution_context 函数，从 /proc/self、cgroup、cpuset、
///          环境变量及容器特征中构建当前进程的执行上下文信息。

#pragma once

#include "sysal/diagnostics.hpp"
#include "sysal/execution_context_info.hpp"
#include "sysal/raw_store.hpp"

namespace sysal::detail
{

/// @brief 从 RawStore 解析当前进程的执行上下文
/// @param raw 原始数据存储
/// @param diag 诊断信息容器，用于记录解析过程中的告警
/// @return 填充后的 ExecutionContextInfo（始终返回有效值）
/// @details 解析内容包括：进程 PID/UID/GID、root 判定、cgroup 版本与路径、
///          cpuset（CPU 与内存节点亲和）、有效能力位、容器类型，以及
///          与计算相关的环境变量（CUDA_VISIBLE_DEVICES 等）。
ExecutionContextInfo parse_execution_context(const RawStore& raw, Diagnostics& diag);

} // namespace sysal::detail
