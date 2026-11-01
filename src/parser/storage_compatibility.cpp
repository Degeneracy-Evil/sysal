#include "parser/storage_compatibility.hpp"
#include "parser/parse_utils.hpp"

#include <cctype>
#include <map>

namespace sysal::detail
{
    namespace
    {
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

    void apply_storage_compatibility(StorageDevice &device, std::string_view sysfs_target)
    {
        device.layer = device.partition_number                                      ? "partition"
                       : !device.mapper_uuid.empty() || !device.mapper_name.empty() ? "device-mapper"
                       : !device.raid_level.empty()                                 ? "md"
                       : sysfs_target.find("/virtual/") != std::string_view::npos   ? "virtual"
                       : sysfs_target.empty()                                       ? "unknown"
                                                                                    : "disk";
        // Historical name heuristics are confined to deprecated compatibility output.
        device.kind = StorageKind::Other;
        if(device.name.value.starts_with("nvme"))
        {
            device.kind = StorageKind::Nvme;
            return;
        }
        for(const auto prefix : {"loop", "ram", "sr", "dm-", "md", "zram", "fd"})
            if(device.name.value.starts_with(prefix))
                return;
        if(device.rotational)
            device.kind = *device.rotational ? StorageKind::Hdd : StorageKind::Ssd;
    }
    void apply_legacy_storage_mounts(Storage &storage, const RawStore &raw, std::vector<std::string> &warnings)
    {
        if(raw.has_success(RawSource::StorageMountInfo))
            return;
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

        for(auto &dev : storage.devices)
        {
            // 合并 df -Th 数据：精确匹配或分区前缀匹配
            auto df_it = df_map.find(dev.name.value);
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
                    if(is_partition_of(dev.name.value, df_name))
                    {
                        if(!dev.mount_point.has_value() || df_info.first == "/")
                        {
                            dev.mount_point = MountPoint{df_info.first};
                            dev.fs_type = FilesystemType{df_info.second};
                        }
                    }
                }
            }
        }
    }
} // namespace sysal::detail
