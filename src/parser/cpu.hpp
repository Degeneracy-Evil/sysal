/// @file cpu.hpp
/// @brief CPU 信息解析器
/// @details 从 RawStore 中解析 Cpu 结构体，包括物理封装、物理核、
///          逻辑 CPU、NUMA 节点和 ISA 扩展。

#pragma once

#include "sysal/model/cpu.hpp"
#include "sysal/model/raw_store.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal::detail
{

/// @brief 解析 CPU 信息
/// @param raw 原始证据存储（只读）
/// @param warnings 警告列表（追加写入）
/// @return 解析成功返回 Cpu，否则返回 nullopt
std::optional<Cpu> parse_cpu(const RawStore& raw, std::vector<std::string>& warnings);

} // namespace sysal::detail
