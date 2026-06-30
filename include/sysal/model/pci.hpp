/// @file pci.hpp
/// @brief PCI 数据模型
/// @details 定义 PCI 子系统的数据结构：PciDevice、Pci，
///          描述系统中存在的 PCI 设备清单，并提供按地址查找接口。

#pragma once

#include "sysal/types/ids.hpp"
#include "sysal/types/value_types.hpp"

#include <optional>
#include <vector>

namespace sysal
{

/// @brief 单个 PCI 设备
struct PciDevice
{
    PciAddress address;                  ///< PCI 地址
    Vendor vendor;                       ///< 厂商
    DeviceName device_name;              ///< 设备名称
    PciClass device_class;               ///< 设备类别
    std::optional<NumaNodeId> numa_node; ///< 所属 NUMA 节点（可能未知）
};

/// @brief PCI 子系统聚合
/// @details 持有全部 PCI 设备并提供按地址查找接口。
struct Pci
{
    std::vector<PciDevice> devices; ///< PCI 设备列表

    /// @brief 按 PCI 地址查找设备
    /// @param addr PCI 地址
    /// @return 指向匹配设备的指针，未找到则返回 nullptr
    const PciDevice* find(PciAddress addr) const;
};

} // namespace sysal
