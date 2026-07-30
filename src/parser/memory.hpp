/// @file memory.hpp
/// @brief 内存信息解析器
/// @details 从 RawStore 中解析 Memory 结构体，包括系统内存总量
///          和各 NUMA 节点的内存分布。

#pragma once

#include "sysal/model/memory.hpp"
#include "sysal/model/raw_store.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal::detail
{

    /// @brief 解析内存信息
    /// @param raw 原始证据存储（只读）
    /// @param warnings 警告列表（追加写入）
    /// @return 解析成功返回 Memory，否则返回 nullopt
    std::optional<Memory> parse_memory(const RawStore &raw, std::vector<std::string> &warnings);

} // namespace sysal::detail
