/// @file storage_parser.cpp
/// @brief 存储子系统解析器实现
/// @details 从 sysfs /sys/block/ 下解析块设备信息，按设备名前缀区分
///          NVMe、SATA 等类型，并读取容量、型号、PCI 地址等属性。

#include "storage_parser.hpp"
#include "parse_utils.hpp"

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <string>
#include <unordered_map>

namespace sysal::detail
{

namespace
{

/// @brief 根据块设备名前缀判定存储类型
/// @param name 块设备名，如 "nvme0n1"、"sda"
/// @return 对应的 StorageKind 枚举值；无法识别时返回 StorageKind::Other
StorageKind classify_storage(const std::string& name)
{
    if(name.starts_with("nvme"))
    {
        return StorageKind::Nvme;
    }
    if(name.starts_with("sd"))
    {
        return StorageKind::Sata;
    }
    return StorageKind::Other;
}

} // namespace

/// @brief 从 RawStore 解析存储子系统信息
/// @param raw 原始数据存储
/// @param diag 诊断信息容器，用于记录解析过程中的告警
/// @return 解析成功返回 StorageSubsystem；无块设备数据或解析失败时返回 std::nullopt
/// @details 流程：
///          1. 筛选 RawSource::SysfsBlock 来源的采集成功记录；
///          2. 构建"路径 -> 内容"映射，提取 /sys/block/ 下的设备名；
///          3. 逐个设备读取 size（扇区数 × 512 = 容量）、device/model（型号）、
///             device（PCI 地址）等属性；
///          4. 用 classify_storage 按名称前缀分类设备类型。
std::optional<StorageSubsystem> parse_storage(const RawStore& raw, Diagnostics& diag)
{
    auto records = raw.get_all(RawSource::SysfsBlock);
    if(records.empty())
    {
        // 没有任何块设备采集记录，直接返回空
        return std::nullopt;
    }

    // 构建 sysfs 块设备的"路径 -> 内容"映射
    auto path_map = build_path_map(raw, RawSource::SysfsBlock);
    // 提取 /sys/block/ 下的唯一设备名列表
    auto device_names = extract_prefix_keys(path_map, "/sys/block/");
    if(device_names.empty())
    {
        // 有记录但提取不出设备名，记录告警
        add_warning(diag, "Block sysfs records exist but no devices parsed", RawSource::SysfsBlock);
        return std::nullopt;
    }

    StorageSubsystem facts;
    std::uint32_t index = 0;
    for(const auto& name : device_names)
    {
        StorageDevice device;
        device.id = StorageId{index};
        device.name = DeviceName{name};
        device.kind = classify_storage(name);

        const auto base = "/sys/block/" + name + "/";

        // 解析容量：sysfs size 文件记录的是 512 字节扇区数
        auto size_it = path_map.find(base + "size");
        if(size_it != path_map.end())
        {
            auto sectors = parse_uint(trim(size_it->second));
            if(sectors)
            {
                device.capacity = MemorySize{*sectors * 512ULL};
            }
        }

        // 解析型号：若存在则用型号覆盖默认的设备名作为显示名
        auto model_it = path_map.find(base + "device/model");
        if(model_it != path_map.end())
        {
            auto model = trim(model_it->second);
            if(!model.empty())
            {
                device.name = DeviceName{model};
            }
        }

        // 解析 PCI 地址：device 文件通常指向 PCI 设备路径
        auto device_it = path_map.find(base + "device");
        if(device_it != path_map.end())
        {
            auto pci_str = trim(device_it->second);
            if(!pci_str.empty())
            {
                device.pci_address = parse_pci_address(pci_str);
            }
        }

        facts.devices.push_back(device);
        ++index;
    }

    return facts;
}

} // namespace sysal::detail
