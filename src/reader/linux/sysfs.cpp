/// @file sysfs.cpp
/// @brief Linux sysfs 采集器实现
/// @details 从 /sys 采集原始数据写入 RawStore，遍历 CPU 拓扑、NUMA 节点、
///          网络接口、PCI 设备、块设备及 DMI 信息。

#include "reader/linux/sysfs.hpp"
#include "reader/linux/file_utils.hpp"

#include <filesystem>
#include <string>

namespace sysal::reader
{

namespace
{

namespace fs = std::filesystem;

/// @brief 读取 sysfs 文件并添加记录，失败时记录 Failed 状态
/// @param raw 原始证据存储
/// @param source 原始数据来源
/// @param path 文件路径
void read_sysfs_file(RawStore& raw, RawSource source, const std::string& path)
{
    auto content = read_file(path);
    if(content)
    {
        add_record(raw, source, path, *content, CollectStatus::Success);
    }
    else
    {
        add_record(raw, source, path, "", CollectStatus::Failed);
    }
}

/// @brief 采集 CPU 拓扑信息
/// @param raw 原始证据存储
/// @details 遍历 /sys/devices/system/cpu/cpuN，读取 topology、online、cpufreq 文件
void read_cpu_sysfs(RawStore& raw)
{
    const fs::path cpu_base = "/sys/devices/system/cpu";
    if(!fs::exists(cpu_base))
    {
        add_record(raw, RawSource::SysfsCpu, cpu_base.string(), "", CollectStatus::Failed);
        return;
    }

    bool found_any = false;
    std::error_code ec;
    for(const auto& entry : fs::directory_iterator(cpu_base, ec))
    {
        if(!entry.is_directory())
        {
            continue;
        }
        auto name = entry.path().filename().string();
        // 仅处理 cpuN 目录
        if(name.size() < 4 || name.substr(0, 3) != "cpu" ||
           name.find_first_not_of("0123456789", 3) != std::string::npos)
        {
            continue;
        }

        found_any = true;
        const auto& dir = entry.path();

        // topology 文件
        read_sysfs_file(raw, RawSource::SysfsCpu,
                        (dir / "topology" / "physical_package_id").string());
        read_sysfs_file(raw, RawSource::SysfsCpu, (dir / "topology" / "core_id").string());

        // online 状态
        read_sysfs_file(raw, RawSource::SysfsCpu, (dir / "online").string());

        // cpufreq 文件
        read_sysfs_file(raw, RawSource::SysfsCpu, (dir / "cpufreq" / "base_frequency").string());
        read_sysfs_file(raw, RawSource::SysfsCpu, (dir / "cpufreq" / "scaling_max_freq").string());
    }

    if(!found_any)
    {
        add_record(raw, RawSource::SysfsCpu, cpu_base.string(), "", CollectStatus::Failed);
    }
}

/// @brief 采集 NUMA 节点信息
/// @param raw 原始证据存储
/// @details 遍历 /sys/devices/system/node/nodeN，读取 cpulist 和 meminfo
void read_numa_sysfs(RawStore& raw)
{
    const fs::path node_base = "/sys/devices/system/node";
    if(!fs::exists(node_base))
    {
        add_record(raw, RawSource::SysfsNuma, node_base.string(), "", CollectStatus::Failed);
        return;
    }

    bool found_any = false;
    std::error_code ec;
    for(const auto& entry : fs::directory_iterator(node_base, ec))
    {
        if(!entry.is_directory())
        {
            continue;
        }
        auto name = entry.path().filename().string();
        if(name.size() < 5 || name.substr(0, 4) != "node" ||
           name.find_first_not_of("0123456789", 4) != std::string::npos)
        {
            continue;
        }

        found_any = true;
        const auto& dir = entry.path();

        read_sysfs_file(raw, RawSource::SysfsNuma, (dir / "cpulist").string());
        read_sysfs_file(raw, RawSource::SysfsNuma, (dir / "meminfo").string());
    }

    if(!found_any)
    {
        add_record(raw, RawSource::SysfsNuma, node_base.string(), "", CollectStatus::Failed);
    }
}

/// @brief 采集网络接口信息
/// @param raw 原始证据存储
/// @details 遍历 /sys/class/net，读取 address、operstate、speed
void read_net_sysfs(RawStore& raw)
{
    const fs::path net_base = "/sys/class/net";
    if(!fs::exists(net_base))
    {
        add_record(raw, RawSource::SysfsNet, net_base.string(), "", CollectStatus::Failed);
        return;
    }

    bool found_any = false;
    std::error_code ec;
    for(const auto& entry : fs::directory_iterator(net_base, ec))
    {
        if(!entry.is_directory() && !entry.is_symlink())
        {
            continue;
        }
        auto name = entry.path().filename().string();
        if(name.empty())
        {
            continue;
        }

        found_any = true;
        const auto& dir = entry.path();

        read_sysfs_file(raw, RawSource::SysfsNet, (dir / "address").string());
        read_sysfs_file(raw, RawSource::SysfsNet, (dir / "operstate").string());
        read_sysfs_file(raw, RawSource::SysfsNet, (dir / "speed").string());
    }

    if(!found_any)
    {
        add_record(raw, RawSource::SysfsNet, net_base.string(), "", CollectStatus::Failed);
    }
}

/// @brief 采集 PCI 设备信息
/// @param raw 原始证据存储
/// @details 遍历 /sys/bus/pci/devices，读取 vendor、device、class、numa_node
void read_pci_sysfs(RawStore& raw)
{
    const fs::path pci_base = "/sys/bus/pci/devices";
    if(!fs::exists(pci_base))
    {
        add_record(raw, RawSource::SysfsPci, pci_base.string(), "", CollectStatus::Failed);
        return;
    }

    bool found_any = false;
    std::error_code ec;
    for(const auto& entry : fs::directory_iterator(pci_base, ec))
    {
        if(!entry.is_directory() && !entry.is_symlink())
        {
            continue;
        }
        auto name = entry.path().filename().string();
        if(name.empty())
        {
            continue;
        }

        found_any = true;
        const auto& dir = entry.path();

        read_sysfs_file(raw, RawSource::SysfsPci, (dir / "vendor").string());
        read_sysfs_file(raw, RawSource::SysfsPci, (dir / "device").string());
        read_sysfs_file(raw, RawSource::SysfsPci, (dir / "class").string());
        read_sysfs_file(raw, RawSource::SysfsPci, (dir / "numa_node").string());
    }

    if(!found_any)
    {
        add_record(raw, RawSource::SysfsPci, pci_base.string(), "", CollectStatus::Failed);
    }
}

/// @brief 采集块设备信息
/// @param raw 原始证据存储
/// @details 遍历 /sys/block，读取 size 和 device/ 子目录
void read_block_sysfs(RawStore& raw)
{
    const fs::path block_base = "/sys/block";
    if(!fs::exists(block_base))
    {
        add_record(raw, RawSource::SysfsBlock, block_base.string(), "", CollectStatus::Failed);
        return;
    }

    bool found_any = false;
    std::error_code ec;
    for(const auto& entry : fs::directory_iterator(block_base, ec))
    {
        if(!entry.is_directory() && !entry.is_symlink())
        {
            continue;
        }
        auto name = entry.path().filename().string();
        if(name.empty())
        {
            continue;
        }

        found_any = true;
        const auto& dir = entry.path();

        read_sysfs_file(raw, RawSource::SysfsBlock, (dir / "size").string());

        // device/ 子目录下的型号等信息
        if(fs::exists(dir / "device"))
        {
            read_sysfs_file(raw, RawSource::SysfsBlock, (dir / "device" / "model").string());
        }
    }

    if(!found_any)
    {
        add_record(raw, RawSource::SysfsBlock, block_base.string(), "", CollectStatus::Failed);
    }
}

/// @brief 采集 DMI/BIOS 信息
/// @param raw 原始证据存储
/// @details 读取 /sys/class/dmi/id 下的固件与产品信息文件
void read_dmi_sysfs(RawStore& raw)
{
    const fs::path dmi_base = "/sys/class/dmi/id";
    if(!fs::exists(dmi_base))
    {
        add_record(raw, RawSource::SysfsDmi, dmi_base.string(), "", CollectStatus::Failed);
        return;
    }

    read_sysfs_file(raw, RawSource::SysfsDmi, (dmi_base / "bios_vendor").string());
    read_sysfs_file(raw, RawSource::SysfsDmi, (dmi_base / "bios_version").string());
    read_sysfs_file(raw, RawSource::SysfsDmi, (dmi_base / "bios_date").string());
    read_sysfs_file(raw, RawSource::SysfsDmi, (dmi_base / "product_name").string());
    read_sysfs_file(raw, RawSource::SysfsDmi, (dmi_base / "product_serial").string());
    read_sysfs_file(raw, RawSource::SysfsDmi, (dmi_base / "sys_vendor").string());
}

} // namespace

void read_sysfs(RawStore& raw, Collect flags)
{
    struct ReaderDispatch
    {
        Collect flag;
        void (*read)(RawStore&);
    };

    static const ReaderDispatch reader_dispatch[] = {
        {Collect::Cpu, read_cpu_sysfs},       {Collect::Memory, read_numa_sysfs},
        {Collect::Network, read_net_sysfs},   {Collect::Pci, read_pci_sysfs},
        {Collect::Storage, read_block_sysfs}, {Collect::Platform, read_dmi_sysfs},
    };

    for(const auto& entry : reader_dispatch)
    {
        if(has(flags, entry.flag))
        {
            entry.read(raw);
        }
    }
}

} // namespace sysal::reader
