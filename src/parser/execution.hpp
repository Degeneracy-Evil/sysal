/// @file execution.hpp
/// @brief 执行上下文解析器
/// @details 从 RawStore 中解析 ExecutionContext 结构体，包括进程信息、环境变量、
///          cgroup、cpuset、权限、容器检测及可见性索引。

#pragma once

#include "sysal/model/execution.hpp"
#include "sysal/model/raw_store.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal::detail
{

/// @brief 解析执行上下文信息
/// @param raw 原始证据存储（只读）
/// @param warnings 警告列表（追加写入）
/// @return 解析成功返回 ExecutionContext，否则返回 nullopt
std::optional<ExecutionContext> parse_execution(const RawStore& raw,
                                                std::vector<std::string>& warnings);

} // namespace sysal::detail
