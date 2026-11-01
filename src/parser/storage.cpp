/// @file storage.cpp
/// @brief 存储解析器实现
/// @details 从 sysfs 块设备数据解析存储设备信息。

#include "storage.hpp"
#include "parser/storage_classification.hpp"
#include "parser/storage_compatibility.hpp"
#include "parser/storage_connections.hpp"

#include "parse_utils.hpp"
#include "parser/filesystems.hpp"
#include "storage_topology.hpp"

#include <limits>
#include <map>
#include <string_view>

namespace sysal::detail
{

    namespace
    {

        /// @brief 从 sysfs 路径提取块设备名称
        /// @param path sysfs 路径（如 "/sys/block/nvme0n1/size"）
        /// @return 块设备名称（如 "nvme0n1"），提取失败则返回空串
        std::string extract_device_name(std::string_view path)
        {
            // 路径格式: /sys/block/DEVNAME/file 或 /sys/block/DEVNAME/subdir/file
            const auto prefix = path.starts_with("/sys/class/block/") ? std::string_view{"/sys/class/block/"}
                                                                      : std::string_view{"/sys/block/"};
            auto block_pos = path.find(prefix);
            if(block_pos == std::string_view::npos)
            {
                return {};
            }
            const auto start = block_pos + prefix.size(); // "/sys/block/" 长度
            // 在原始视图中查找，避免 GCC 11 对子视图长度的错误越界诊断。
            const auto slash_pos = path.find('/', start);
            const auto end = slash_pos == std::string_view::npos ? path.size() : slash_pos;
            return std::string(path.substr(start, end - start));
        }

        /// @brief 从块设备符号链接目标中提取 PCI 地址
        /// @param target 符号链接目标，如 "../devices/pci0000:e2/0000:e2:04.0/0000:e4:00.0/nvme/nvme0/nvme0n1"
        /// @return 最后一个合法 PCI 地址段（即设备所属的 PCI 控制器）；虚拟设备返回 nullopt
        std::optional<PciAddress> extract_pci_address_from_block(std::string_view target)
        {
            std::optional<PciAddress> result;
            for(const auto &segment : split(target, '/'))
            {
                if(auto addr = parse_pci_address(segment))
                {
                    result = addr;
                }
            }
            return result;
        }

    } // namespace

    std::optional<Storage> parse_storage(const RawStore &raw, std::vector<std::string> &warnings)
    {
        auto block_records = raw.get_all(RawSource::SysfsBlock);
        if(block_records.empty() && raw.get_all(RawSource::SysfsStorageConnections).empty() &&
           !raw.has_success(RawSource::StorageMountInfo) && raw.enumeration(InventoryKind::BlockDevices) == nullptr)
        {
            warnings.push_back("parse_storage: 缺少 SysfsBlock 数据");
            return std::nullopt;
        }

        // 按设备名分组：device_name → {filename → payload}
        std::map<std::string, std::map<std::string, std::string>> device_attrs;
        for(const auto *rec : block_records)
        {
            if(rec->status != CollectStatus::Success)
            {
                continue;
            }

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
            if(!raw.has_success(RawSource::StorageMountInfo) && raw.enumeration(InventoryKind::BlockDevices) == nullptr)
                warnings.push_back("parse_storage: SysfsBlock 记录中无有效块设备");
            if(raw.get_all(RawSource::SysfsStorageConnections).empty() &&
               !raw.has_success(RawSource::StorageMountInfo) && raw.enumeration(InventoryKind::BlockDevices) == nullptr)
                return std::nullopt;
        }

        Storage storage;
        std::uint32_t seq = 0;

        for(const auto &[dev_name, attrs] : device_attrs)
        {
            StorageDevice dev;
            dev.id = StorageId{seq};
            dev.name = DeviceName{dev_name};

            // 从 size 文件读取容量（512 字节扇区数）
            auto size_it = attrs.find("size");
            if(size_it != attrs.end())
            {
                auto sectors = parse_uint(trim(size_it->second));
                if(sectors.has_value() && *sectors <= std::numeric_limits<std::uint64_t>::max() / 512)
                {
                    dev.capacity = MemorySize{*sectors * 512};
                }
                else
                {
                    warnings.push_back("parse_storage: 块设备 " + dev_name +
                                       " 的 size 解析失败: " + trim(size_it->second));
                }
            }

            // 从设备入口符号链接目标提取 PCI 地址（虚拟设备无，保持 nullopt）
            auto dev_pci_it = attrs.find("device");
            if(dev_pci_it != attrs.end())
            {
                if(auto pci = extract_pci_address_from_block(dev_pci_it->second))
                {
                    dev.pci_address = pci;
                }
            }

            const auto attribute = [&](const char *field)
            {
                const auto found = attrs.find(field);
                return found == attrs.end() ? std::string{} : trim(found->second);
            };
            dev.device_number = parse_device_number(attribute("dev"));
            const auto partition = parse_uint(attribute("partition"));
            if(partition && *partition > 0 && *partition <= std::numeric_limits<std::uint32_t>::max())
                dev.partition_number = static_cast<std::uint32_t>(*partition);
            const auto parent = attribute("parent");
            if(!parent.empty())
                dev.parent = DeviceName{parent};
            for(const auto &slave : split(attribute("slaves"), '\n'))
                if(!slave.empty())
                    dev.slaves.push_back(DeviceName{slave});
            dev.mapper_name = hardware_text(attribute("name"));
            dev.mapper_uuid = hardware_text(attribute("uuid"));
            dev.raid_level = hardware_text(attribute("level"));
            dev.raid_state = hardware_text(attribute("array_state"));
            for(const auto &[field, destination] :
                {std::pair{"raid_disks", &dev.raid_disks}, std::pair{"degraded", &dev.raid_degraded}})
                if(auto number = parse_uint(attribute(field));
                   number && *number <= std::numeric_limits<std::uint32_t>::max())
                    *destination = static_cast<std::uint32_t>(*number);
            const auto target = attribute("device");
            dev.classification.layer =
                dev.partition_number                                       ? BlockDeviceLayer::Partition
                : attrs.contains("partition")                              ? BlockDeviceLayer::Unknown
                : attrs.contains("name") || attrs.contains("uuid")         ? BlockDeviceLayer::DeviceMapper
                : attrs.contains("level") || attrs.contains("array_state") ? BlockDeviceLayer::Raid
                : attribute("loop_present") == "1"                         ? BlockDeviceLayer::Loop
                : target.find("/virtual/") != std::string::npos            ? BlockDeviceLayer::Virtual
                : target.empty()                                           ? BlockDeviceLayer::Unknown
                                                                           : BlockDeviceLayer::Device;
            dev.driver = hardware_text(attribute("driver"));

            const std::pair<std::string_view, std::string *> text_fields[] = {{"model", &dev.model},
                                                                              {"vendor", &dev.vendor.value},
                                                                              {"serial", &dev.serial},
                                                                              {"rev", &dev.firmware_revision},
                                                                              {"firmware_rev", &dev.firmware_revision},
                                                                              {"wwid", &dev.wwid},
                                                                              {"transport", &dev.transport}};
            for(const auto &[field, destination] : text_fields)
            {
                if(auto it = attrs.find(std::string(field)); it != attrs.end())
                {
                    auto value = hardware_text(it->second);
                    if(!value.empty())
                    {
                        *destination = std::move(value);
                    }
                }
            }
            const std::pair<std::string_view, std::optional<MemorySize> *> sizes[] = {
                {"logical_block_size", &dev.logical_block_size},
                {"physical_block_size", &dev.physical_block_size},
                {"minimum_io_size", &dev.minimum_io_size},
                {"optimal_io_size", &dev.optimal_io_size}};
            for(const auto &[field, destination] : sizes)
            {
                if(auto it = attrs.find(std::string(field)); it != attrs.end())
                {
                    if(auto number = parse_uint(trim(it->second)))
                    {
                        *destination = MemorySize{*number};
                    }
                }
            }
            const std::pair<std::string_view, std::optional<bool> *> states[] = {
                {"rotational", &dev.rotational}, {"ro", &dev.read_only}, {"removable", &dev.removable}};
            for(const auto &[field, destination] : states)
            {
                if(auto it = attrs.find(std::string(field)); it != attrs.end())
                {
                    auto value = trim(it->second);
                    if(value == "0" || value == "1")
                    {
                        *destination = value == "1";
                    }
                }
            }
            if(auto it = attrs.find("numa_node"); it != attrs.end())
            {
                if(auto number = parse_uint(trim(it->second));
                   number && *number <= std::numeric_limits<std::uint32_t>::max())
                {
                    dev.numa_node = NumaNodeId{static_cast<std::uint32_t>(*number)};
                }
            }
            if(auto it = attrs.find("scheduler"); it != attrs.end())
            {
                const auto start = it->second.find('[');
                const auto end = it->second.find(']', start);
                if(start != std::string::npos && end != std::string::npos)
                {
                    dev.scheduler = it->second.substr(start + 1, end - start - 1);
                }
                else if(trim(it->second) == "none")
                {
                    dev.scheduler = "none";
                }
            }

            apply_storage_compatibility(dev, target);
            storage.devices.push_back(std::move(dev));
            ++seq;
        }

        apply_storage_connections(storage, raw);
        apply_storage_classification(storage);
        apply_legacy_storage_mounts(storage, raw, warnings);
        apply_storage_mounts(storage, raw, warnings);
        apply_filesystems(storage, raw, warnings);
        return storage;
    }

} // namespace sysal::detail
