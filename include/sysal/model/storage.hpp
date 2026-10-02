/// @file storage.hpp
/// @brief 存储数据模型
/// @details 定义存储子系统的数据结构：StorageDevice、Storage，
///          描述系统中的块设备信息。

#pragma once

#include "sysal/types/enums.hpp"
#include "sysal/types/ids.hpp"
#include "sysal/types/units.hpp"
#include "sysal/types/value_types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal
{

    /// @brief 单个存储设备
    struct StorageDevice
    {
        StorageId id;                          ///< 存储设备 ID
        DeviceName name;                       ///< 设备名称
        std::optional<MemorySize> capacity;    ///< 容量（可能未知）
        std::optional<PciAddress> pci_address; ///< PCI 地址（可能无）
        StorageKind kind{};                    ///< 存储类型
        std::optional<MountPoint> mount_point; ///< 挂载点
        std::optional<FilesystemType> fs_type; ///< 文件系统类型
        std::string model{};
        Vendor vendor{};
        std::string serial{};
        std::string firmware_revision{};
        std::string wwid{};
        std::optional<MemorySize> logical_block_size{};
        std::optional<MemorySize> physical_block_size{};
        std::optional<MemorySize> minimum_io_size{};
        std::optional<MemorySize> optimal_io_size{};
        std::optional<bool> rotational{};
        std::optional<bool> read_only{};
        std::optional<bool> removable{};
        std::optional<NumaNodeId> numa_node{};
        std::string transport{};       ///< 仅保留驱动明确报告的 transport
        std::string controller_name{}; ///< 关联 PCI 控制器名称
        std::string scheduler{};       ///< 当前内核 I/O scheduler
    };

    /// @brief 存储子系统聚合
    struct Storage
    {
        std::vector<StorageDevice> devices; ///< 存储设备列表
    };

} // namespace sysal
