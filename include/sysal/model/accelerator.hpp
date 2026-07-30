/// @file accelerator.hpp
/// @brief 加速器数据模型
/// @details 定义加速器子系统的数据结构：AcceleratorDevice、Accelerators，
///          描述 GPU、NPU、FPGA 等异构加速设备，并提供按类型筛选、
///          可见性筛选与查找接口。

#pragma once

#include "sysal/types/enums.hpp"
#include "sysal/types/ids.hpp"
#include "sysal/types/units.hpp"
#include "sysal/types/value_types.hpp"

#include <optional>
#include <vector>

namespace sysal
{

    /// @brief 单个加速器设备
    struct AcceleratorDevice
    {
        AcceleratorId id;                            ///< 加速器 ID
        AcceleratorKind kind{};                      ///< 加速器类型
        Vendor vendor;                               ///< 厂商
        DeviceName name;                             ///< 设备名称
        std::optional<PciAddress> pci_address;       ///< PCI 地址（可能无）
        std::optional<NumaNodeId> nearest_numa_node; ///< 最近 NUMA 节点（可能未知）
        std::optional<MemorySize> memory_size;       ///< 设备显存/内存（可能未知）
        std::optional<DriverId> driver;              ///< 关联驱动 ID（可能无）
        bool visible_to_current_process{};           ///< 当前进程是否可见
    };

    /// @brief 加速器子系统聚合
    /// @details 持有全部加速器设备并提供按类型筛选、可见性筛选与查找接口。
    struct Accelerators
    {
        std::vector<AcceleratorDevice> devices; ///< 加速器设备列表

        /// @brief 按类型筛选加速器
        /// @param kind 加速器类型
        /// @return 指向匹配设备的指针向量
        std::vector<const AcceleratorDevice *> by_kind(AcceleratorKind kind) const;

        /// @brief 获取全部 GPU
        /// @return 指向 GPU 设备的指针向量
        std::vector<const AcceleratorDevice *> gpus() const;

        /// @brief 获取全部 NPU
        /// @return 指向 NPU 设备的指针向量
        std::vector<const AcceleratorDevice *> npus() const;

        /// @brief 获取全部 FPGA
        /// @return 指向 FPGA 设备的指针向量
        std::vector<const AcceleratorDevice *> fpgas() const;

        /// @brief 获取当前进程可见的加速器
        /// @return 指向可见设备的指针向量
        std::vector<const AcceleratorDevice *> visible() const;

        /// @brief 按加速器 ID 查找设备
        /// @param id 加速器 ID
        /// @return 指向匹配设备的指针，未找到则返回 nullptr
        const AcceleratorDevice *find(AcceleratorId id) const;
    };

} // namespace sysal
