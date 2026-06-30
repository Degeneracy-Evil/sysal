/// @file topology_info.hpp
/// @brief 拓扑信息数据模型
/// @details 描述 NUMA 关系、PCI 父子关系与设备 NUMA 局部性等拓扑结构。

#pragma once

#include "sysal/ids.hpp"
#include "sysal/units.hpp"
#include "sysal/value_types.hpp"

#include <vector>

namespace sysal
{

/// @brief NUMA 节点关系
/// @details 描述一个 NUMA 节点包含的 CPU 封装及本地内存。
struct NumaRelation
{
    NumaNodeId node;                        ///< NUMA 节点 ID
    std::vector<CpuPackageId> packages;     ///< 该节点包含的 CPU 封装列表
    std::optional<MemorySize> local_memory; ///< 本地内存大小（可能未知）
};

/// @brief PCI 设备父子关系
struct PciRelation
{
    PciAddress parent; ///< 父设备 PCI 地址（如 root port/switch）
    PciAddress child;  ///< 子设备 PCI 地址
};

/// @brief 设备 NUMA 局部性
/// @details 描述某 PCI 设备最近的 NUMA 节点，用于评估设备与 CPU/内存的亲和度。
struct DeviceLocality
{
    PciAddress pci_address;       ///< PCI 设备地址
    NumaNodeId nearest_numa_node; ///< 最近 NUMA 节点
};

/// @brief 拓扑信息聚合
/// @details 汇总 NUMA 关系、PCI 关系与设备局部性。
struct TopologyInfo
{
    std::vector<NumaRelation> numa_relations;      ///< NUMA 节点关系列表
    std::vector<PciRelation> pci_relations;        ///< PCI 父子关系列表
    std::vector<DeviceLocality> device_localities; ///< 设备局部性列表
};

} // namespace sysal
