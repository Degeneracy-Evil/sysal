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
    StorageId id;                           ///< 存储设备 ID
    DeviceName name;                        ///< 设备名称
    std::optional<MemorySize> capacity;     ///< 容量（可能未知）
    std::optional<PciAddress> pci_address;  ///< PCI 地址（可能无）
    StorageKind kind{};                     ///< 存储类型
    std::optional<std::string> mount_point; ///< 挂载点
    std::optional<std::string> fs_type;     ///< 文件系统类型
};

/// @brief 存储子系统聚合
struct Storage
{
    std::vector<StorageDevice> devices; ///< 存储设备列表
};

} // namespace sysal
