/// @file memory.hpp
/// @brief 内存数据模型
/// @details 定义内存子系统的数据结构：NumaMemory、DimmInfo、Memory，
///          描述系统内存总量、各 NUMA 节点的内存分布以及 DIMM 内存条详情。

#pragma once

#include "sysal/types/ids.hpp"
#include "sysal/types/units.hpp"
#include "sysal/types/value_types.hpp"

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

/// @brief 单条 DIMM 内存条信息
struct DimmInfo
{
    std::string locator;                      ///< 物理插槽定位符（如 CPU0_C0D0）
    std::string bank_locator;                 ///< 内存库定位符（如 NODE 0）
    MemorySize size{0};                       ///< 容量（字节）
    std::optional<TransferRate> speed_mts;    ///< 标称速率（MT/s）
    std::optional<Vendor> manufacturer;       ///< 制造商
    std::optional<std::string> part_number;   ///< 部件号
    std::optional<std::uint32_t> rank;        ///< rank 数
    std::optional<std::uint32_t> total_width; ///< 总位宽（含 ECC）
    std::optional<std::uint32_t> data_width;  ///< 数据位宽
    std::optional<std::string> form_factor;   ///< 外形规格（如 DIMM）
    bool present{};                           ///< 插槽是否已安装内存条
};

/// @brief 内存子系统聚合
struct Memory
{
    MemorySize total_memory;                          ///< 系统内存总量
    std::optional<MemorySize> available_memory;       ///< 可用内存（可能未知）
    std::string memory_type;                          ///< 内存类型（如 DDR4、DDR5）
    std::optional<TransferRate> configured_speed_mts; ///< 实际配置速率（MT/s）
    std::vector<NumaMemory> numa_memory;              ///< 各 NUMA 节点内存信息
    std::vector<DimmInfo> dimms;                      ///< 各 DIMM 内存条信息
    std::optional<std::uint32_t> dimm_count;          ///< DIMM 插槽总数
    std::optional<std::uint32_t> populated_dimms;     ///< 已安装内存条的 DIMM 数
};

} // namespace sysal
