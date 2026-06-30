#include "storage.hpp"

#include "parse_utils.hpp"

#include <map>
#include <string_view>

namespace sysal::detail
{

namespace
{

/// @brief 从块设备名称推断存储类型
/// @param name 块设备名称（如 "nvme0n1"、"sda"）
/// @return 推断的存储类型
StorageKind infer_storage_kind(std::string_view name)
{
    if(name.find("nvme") == 0)
    {
        return StorageKind::Nvme;
    }
    if(name.find("sd") == 0)
    {
        return StorageKind::Sata;
    }
    return StorageKind::Other;
}

/// @brief 从 sysfs 路径提取块设备名称
/// @param path sysfs 路径（如 "/sys/block/nvme0n1/size"）
/// @return 块设备名称（如 "nvme0n1"），提取失败则返回空串
std::string extract_device_name(std::string_view path)
{
    // 路径格式: /sys/block/DEVNAME/file 或 /sys/block/DEVNAME/subdir/file
    auto block_pos = path.find("/sys/block/");
    if(block_pos == std::string_view::npos)
    {
        return {};
    }
    auto after_block = path.substr(block_pos + 11); // "/sys/block/" 长度
    auto slash_pos = after_block.find('/');
    if(slash_pos == std::string_view::npos)
    {
        return std::string(after_block);
    }
    return std::string(after_block.substr(0, slash_pos));
}

/// @brief 从 sysfs 路径提取文件名
/// @param path sysfs 路径（如 "/sys/block/nvme0n1/size"）
/// @return 文件名（如 "size"）
std::string extract_filename(std::string_view path)
{
    auto slash_pos = path.rfind('/');
    if(slash_pos == std::string_view::npos)
    {
        return std::string(path);
    }
    return std::string(path.substr(slash_pos + 1));
}

} // namespace

std::optional<Storage> parse_storage(const RawStore& raw, std::vector<std::string>& warnings)
{
    auto block_records = raw.get_all(RawSource::SysfsBlock);
    if(block_records.empty())
    {
        // 无 sysfs block 数据，表示未检测到存储设备
        return std::nullopt;
    }

    // 按设备名分组：device_name → {filename → payload}
    std::map<std::string, std::map<std::string, std::string>> device_attrs;
    for(const auto* rec : block_records)
    {
        auto dev_name = extract_device_name(rec->path_or_command);
        if(dev_name.empty())
        {
            continue;
        }
        auto filename = extract_filename(rec->path_or_command);
        device_attrs[dev_name][filename] = rec->payload;
    }

    if(device_attrs.empty())
    {
        warnings.push_back("parse_storage: SysfsBlock 记录中无有效块设备");
        return std::nullopt;
    }

    Storage storage;
    std::uint32_t seq = 0;

    for(const auto& [dev_name, attrs] : device_attrs)
    {
        StorageDevice dev;
        dev.id = StorageId{seq};
        dev.name = DeviceName{dev_name};
        dev.kind = infer_storage_kind(dev_name);

        // 从 size 文件读取容量（512 字节扇区数）
        auto size_it = attrs.find("size");
        if(size_it != attrs.end())
        {
            auto sectors = parse_uint(trim(size_it->second));
            if(sectors.has_value())
            {
                dev.capacity = MemorySize{*sectors * 512};
            }
            else
            {
                warnings.push_back("parse_storage: 块设备 " + dev_name +
                                   " 的 size 解析失败: " + trim(size_it->second));
            }
        }

        // B-2 修正：PCI 地址暂不可用，留空并发出警告
        // 未来可从 device/symlink 路径解析 PCI 地址
        auto symlink_it = attrs.find("device");
        if(symlink_it == attrs.end())
        {
            warnings.push_back("parse_storage: 块设备 " + dev_name +
                               " 无 PCI 地址信息（B-2 待修正）");
        }

        storage.devices.push_back(std::move(dev));
        ++seq;
    }

    return storage;
}

} // namespace sysal::detail
