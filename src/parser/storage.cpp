/// @file storage.cpp
/// @brief 存储解析器实现
/// @details 从 sysfs 块设备数据解析存储设备信息。

#include "storage.hpp"

#include "parse_utils.hpp"

#include <cctype>
#include <map>
#include <string_view>

namespace sysal::detail
{

    namespace
    {

        /// @brief 从块设备名称与 rotational 属性推断存储类型
        /// @param name 块设备名称（如 "nvme0n1"、"sda"）
        /// @param rotational sysfs queue/rotational 内容（"0"=SSD, "1"=HDD），可能为空
        /// @return 推断的存储类型
        StorageKind infer_storage_kind(std::string_view name, const std::string &rotational)
        {
            if(name.find("nvme") == 0)
            {
                return StorageKind::Nvme;
            }
            // 虚拟设备不按 rotational 分类
            static constexpr std::string_view virtual_prefixes[] = {"loop", "ram", "sr", "dm-", "md", "zram", "fd"};
            for(auto prefix : virtual_prefixes)
            {
                if(name.find(prefix) == 0)
                {
                    return StorageKind::Other;
                }
            }
            if(rotational == "0")
            {
                return StorageKind::Ssd;
            }
            if(rotational == "1")
            {
                return StorageKind::Hdd;
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

        /// @brief 判断 df 设备名是否为某块设备的子分区
        /// @param dev_name 块设备名，如 "sda" 或 "nvme0n1"
        /// @param df_name df -Th 输出的设备名，如 "sda1"、"sda2"、"nvme0n1p1"
        /// @return 当 df_name 以 dev_name 开头且后缀符合分区命名（数字，或 NVMe 的 p+数字）时返回 true
        /// @details SATA 分区后缀为纯数字（sda1），NVMe 分区后缀为 p+数字（nvme0n1p1）。
        bool is_partition_of(std::string_view dev_name, std::string_view df_name)
        {
            if(df_name.size() <= dev_name.size() || !df_name.starts_with(dev_name))
            {
                return false;
            }
            auto suffix = df_name.substr(dev_name.size());
            if(std::isdigit(static_cast<unsigned char>(suffix[0])))
            {
                return true;
            }
            // NVMe 分区：p 后接数字（nvme0n1p1）
            return suffix.size() > 1 && suffix[0] == 'p' && std::isdigit(static_cast<unsigned char>(suffix[1]));
        }

    } // namespace

    std::optional<Storage> parse_storage(const RawStore &raw, std::vector<std::string> &warnings)
    {
        auto block_records = raw.get_all(RawSource::SysfsBlock);
        if(block_records.empty())
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
            warnings.push_back("parse_storage: SysfsBlock 记录中无有效块设备");
            return std::nullopt;
        }

        // 解析 df -Th 输出：设备名 → {mount_point, fs_type}
        std::map<std::string, std::pair<std::string, std::string>> df_map;
        auto df_records = raw.get_all(RawSource::DfTh);
        for(const auto *rec : df_records)
        {
            if(rec->status != CollectStatus::Success)
            {
                continue;
            }
            auto lines = split(rec->payload, '\n');
            for(const auto &line : lines)
            {
                auto trimmed = trim(line);
                if(trimmed.empty() || trimmed.starts_with("Filesystem"))
                {
                    continue;
                }
                auto tokens = split(trimmed, ' ');
                std::vector<std::string> fields;
                for(const auto &t : tokens)
                {
                    auto v = trim(t);
                    if(!v.empty())
                    {
                        fields.push_back(v);
                    }
                }
                if(fields.size() < 7)
                {
                    warnings.push_back("parse_storage: df -Th 行字段不足 7 个: " + trimmed);
                    continue;
                }
                // fields[0]=Filesystem, [1]=Type, [2]=Size, [3]=Used, [4]=Avail, [5]=Use%, [6+]=Mounted
                // on
                std::string fs = fields[0];
                std::string type = fields[1];
                std::string mount;
                for(std::size_t i = 6; i < fields.size(); ++i)
                {
                    if(!mount.empty())
                    {
                        mount += ' ';
                    }
                    mount += fields[i];
                }
                // 去掉 /dev/ 前缀
                if(fs.starts_with("/dev/"))
                {
                    fs = fs.substr(5);
                }
                df_map[fs] = {mount, type};
            }
        }

        Storage storage;
        std::uint32_t seq = 0;

        for(const auto &[dev_name, attrs] : device_attrs)
        {
            StorageDevice dev;
            dev.id = StorageId{seq};
            dev.name = DeviceName{dev_name};

            auto rot_it = attrs.find("rotational");
            std::string rotational = (rot_it != attrs.end()) ? trim(rot_it->second) : std::string{};
            dev.kind = infer_storage_kind(dev_name, rotational);

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

            // 从设备入口符号链接目标提取 PCI 地址（虚拟设备无，保持 nullopt）
            auto dev_pci_it = attrs.find("device");
            if(dev_pci_it != attrs.end())
            {
                if(auto pci = extract_pci_address_from_block(dev_pci_it->second))
                {
                    dev.pci_address = pci;
                }
            }

            // 合并 df -Th 数据：精确匹配或分区前缀匹配
            auto df_it = df_map.find(dev_name);
            if(df_it != df_map.end())
            {
                dev.mount_point = MountPoint{df_it->second.first};
                dev.fs_type = FilesystemType{df_it->second.second};
            }
            else
            {
                // 分区匹配：df 显示 sda1/sda2 或 nvme0n1p1，sysfs 显示 sda/nvme0n1
                // 遍历所有匹配分区，优先选择挂载点为 "/" 的根分区
                for(const auto &[df_name, df_info] : df_map)
                {
                    if(is_partition_of(dev_name, df_name))
                    {
                        if(!dev.mount_point.has_value() || df_info.first == "/")
                        {
                            dev.mount_point = MountPoint{df_info.first};
                            dev.fs_type = FilesystemType{df_info.second};
                        }
                    }
                }
            }

            storage.devices.push_back(std::move(dev));
            ++seq;
        }

        return storage;
    }

} // namespace sysal::detail
