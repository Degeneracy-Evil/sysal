/// @file cpu.hpp
/// @brief CPU 数据模型
/// @details 定义 CPU 子系统的数据结构：CpuPackage、CpuCore、LogicalCpu、
///          NumaNode、Cpu，以及层级关系查询与可见性筛选接口。

#pragma once

#include "sysal/types/enums.hpp"
#include "sysal/types/ids.hpp"
#include "sysal/types/units.hpp"
#include "sysal/types/value_types.hpp"

#include <optional>
#include <vector>

namespace sysal
{

/// @brief CPU 物理封装（socket）
struct CpuPackage
{
    CpuPackageId id;                         ///< 封装 ID
    Vendor vendor;                           ///< 厂商
    DeviceName model_name;                   ///< 型号名称
    std::uint32_t physical_cores{};          ///< 物理核数
    std::uint32_t logical_threads{};         ///< 逻辑线程数
    std::optional<Frequency> base_frequency; ///< 基础频率（可能未知）
    std::optional<Frequency> max_frequency;  ///< 最大频率（可能未知）
};

/// @brief CPU 物理核
struct CpuCore
{
    CpuCoreId id;                        ///< 物理核 ID
    CpuPackageId package_id;             ///< 所属封装 ID
    std::uint32_t logical_threads{};     ///< 该核上的逻辑线程数
    std::optional<NumaNodeId> numa_node; ///< 所属 NUMA 节点（可能未知）
};

/// @brief 逻辑 CPU（硬件线程）
struct LogicalCpu
{
    LogicalCpuId id;                     ///< 逻辑 CPU ID
    CpuCoreId core_id;                   ///< 所属物理核 ID
    CpuPackageId package_id;             ///< 所属封装 ID（反范式化）
    std::optional<NumaNodeId> numa_node; ///< 所属 NUMA 节点（可能未知）
    bool visible_to_current_process{};   ///< 当前进程是否可见
};

/// @brief 单个 NUMA 节点
struct NumaNode
{
    NumaNodeId id;                  ///< NUMA 节点 ID
    std::vector<LogicalCpuId> cpus; ///< 该节点包含的逻辑 CPU 列表
};

/// @brief CPU 子系统聚合
/// @details 持有封装、物理核、逻辑 CPU、NUMA 节点与 ISA 扩展列表，
///          并提供层级关系查询与可见性筛选接口。
struct Cpu
{
    Arch arch{};                              ///< CPU 架构
    std::vector<CpuPackage> packages;         ///< 物理封装列表
    std::vector<CpuCore> cores;               ///< 物理核列表
    std::vector<LogicalCpu> logical_cpus;     ///< 逻辑 CPU 列表
    std::vector<NumaNode> numa_nodes;         ///< NUMA 节点列表
    std::vector<IsaExtension> isa_extensions; ///< 支持的 ISA 扩展列表

    /// @brief 按封装 ID 查找封装
    /// @param id 封装 ID
    /// @return 指向匹配封装的指针，未找到则返回 nullptr
    const CpuPackage* find_package(CpuPackageId id) const;

    /// @brief 按物理核 ID 查找物理核
    /// @param id 物理核 ID
    /// @return 指向匹配物理核的指针，未找到则返回 nullptr
    const CpuCore* find_core(CpuCoreId id) const;

    /// @brief 按逻辑 CPU ID 查找逻辑 CPU
    /// @param id 逻辑 CPU ID
    /// @return 指向匹配逻辑 CPU 的指针，未找到则返回 nullptr
    const LogicalCpu* find_logical_cpu(LogicalCpuId id) const;

    /// @brief 获取指定封装下的全部逻辑 CPU
    /// @param id 封装 ID
    /// @return 指向匹配逻辑 CPU 的指针向量
    std::vector<const LogicalCpu*> logical_cpus_of_package(CpuPackageId id) const;

    /// @brief 获取指定物理核上的全部逻辑 CPU
    /// @param id 物理核 ID
    /// @return 指向匹配逻辑 CPU 的指针向量
    std::vector<const LogicalCpu*> logical_cpus_of_core(CpuCoreId id) const;

    /// @brief 获取指定封装下的全部物理核
    /// @param id 封装 ID
    /// @return 指向匹配物理核的指针向量
    std::vector<const CpuCore*> cores_of_package(CpuPackageId id) const;

    /// @brief 获取当前进程可见的全部逻辑 CPU
    /// @return 指向可见逻辑 CPU 的指针向量
    std::vector<const LogicalCpu*> visible_logical_cpus() const;
};

} // namespace sysal
