/// @file memory.hpp
/// @brief 内存数据模型
/// @details 定义内存子系统的数据结构：NumaMemory、Memory，
///          描述系统内存总量与各 NUMA 节点的内存分布。

#pragma once

#include "sysal/types/ids.hpp"
#include "sysal/types/units.hpp"

#include <optional>
#include <vector>

namespace sysal
{

/// @brief 单个 NUMA 节点的内存信息
struct NumaMemory
{
    NumaNodeId node;                     ///< NUMA 节点 ID
    MemorySize total;                    ///< 节点内存总量
    std::optional<MemorySize> available; ///< 可用内存（可能未知）
};

/// @brief 内存子系统聚合
struct Memory
{
    MemorySize total_memory;                    ///< 系统内存总量
    std::optional<MemorySize> available_memory; ///< 可用内存（可能未知）
    std::vector<NumaMemory> numa_memory;        ///< 各 NUMA 节点内存信息
};

} // namespace sysal
