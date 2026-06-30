/// @file sysfs_reader.cpp
/// @brief sysfs 采集器实现
/// @details 实现 read_sysfs，遍历 /sys/devices/system/cpu、/sys/devices/system/node、
///          /sys/class/net、/sys/bus/pci/devices、/sys/block 等目录，
///          采集 CPU 拓扑、NUMA、网络接口、PCI 设备、块设备的原始属性文件内容。

#include "sysfs_reader.hpp"
#include "file_utils.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>

namespace sysal::detail
{

namespace
{

namespace fs = std::filesystem;

/// @brief 判断目录名是否为 cpu<N> 形式的 CPU 目录
/// @param name 目录名
/// @return 是 cpu 加纯数字后缀返回 true，否则 false
bool is_cpu_dir(const std::string& name)
{
    if(!name.starts_with("cpu"))
    {
        return false;
    }
    auto num_part = std::string_view(name).substr(3);
    if(num_part.empty())
    {
        // 排除 "cpu" 本身（无编号后缀）
        return false;
    }
    // 后缀必须全部为数字，unsigned char 避免 signed char 的 UB
    return std::ranges::all_of(num_part, [](unsigned char c) { return std::isdigit(c); });
}

/// @brief 遍历 sysfs 目录并对每个条目执行回调
/// @param base_path 要遍历的目录路径
/// @param read_entry 对每个通过过滤的条目调用的回调（参数：条目名、条目完整路径）
/// @param filter 可选的过滤函数，返回 false 则跳过该条目
void read_sysfs_dir(std::string_view base_path,
                    const std::function<void(const std::string& entry_name,
                                             const std::string& entry_path)>& read_entry,
                    const std::function<bool(const std::string&)>& filter = {})
{
    std::error_code ec;
    for(const auto& entry : fs::directory_iterator(std::string(base_path), ec))
    {
        const auto name = entry.path().filename().string();
        if(filter && !filter(name))
        {
            // 被过滤器排除
            continue;
        }
        read_entry(name, entry.path().string());
    }
}

/// @brief 采集 CPU 拓扑信息（physical_package_id、core_id、thread/core siblings、numa_node）
/// @param raw 目标 RawStore
/// @param now 采集时间点
void read_sysfs_cpu(RawStore& raw, std::chrono::system_clock::time_point now)
{
    const std::string base = "/sys/devices/system/cpu";

    // 在线 CPU 列表
    auto online = read_file(base + "/online");
    add_record(raw, RawSource::SysfsCpu, base + "/online", online, now);

    read_sysfs_dir(
        base,
        [&](const auto& /*name*/, const auto& cpu_path)
        {
            // 物理 CPU 封装号（socket）
            auto pkg_id = read_file(cpu_path + "/topology/physical_package_id");
            add_record(raw, RawSource::SysfsCpu, cpu_path + "/topology/physical_package_id", pkg_id,
                       now);

            // 物理核编号
            auto core_id = read_file(cpu_path + "/topology/core_id");
            add_record(raw, RawSource::SysfsCpu, cpu_path + "/topology/core_id", core_id, now);

            // 同一物理核的超线程兄弟列表
            auto thread_siblings = read_file(cpu_path + "/topology/thread_siblings_list");
            add_record(raw, RawSource::SysfsCpu, cpu_path + "/topology/thread_siblings_list",
                       thread_siblings, now);

            // 同一封装内的所有核兄弟列表
            auto core_siblings = read_file(cpu_path + "/topology/core_siblings_list");
            add_record(raw, RawSource::SysfsCpu, cpu_path + "/topology/core_siblings_list",
                       core_siblings, now);

            // 该 CPU 所属的 NUMA 节点号
            auto numa_node = read_file(cpu_path + "/numa_node");
            add_record(raw, RawSource::SysfsCpu, cpu_path + "/numa_node", numa_node, now);
        },
        is_cpu_dir);
}

/// @brief 采集 NUMA 节点信息（每节点的 meminfo 与 cpulist）
/// @param raw 目标 RawStore
/// @param now 采集时间点
void read_sysfs_numa(RawStore& raw, std::chrono::system_clock::time_point now)
{
    read_sysfs_dir(
        "/sys/devices/system/node",
        [&](const auto& /*name*/, const auto& node_path)
        {
            // 节点内存信息
            auto meminfo = read_file(node_path + "/meminfo");
            add_record(raw, RawSource::SysfsNuma, node_path + "/meminfo", meminfo, now);

            // 节点关联的 CPU 列表
            auto cpulist = read_file(node_path + "/cpulist");
            add_record(raw, RawSource::SysfsNuma, node_path + "/cpulist", cpulist, now);
        },
        [](const std::string& name) { return name.starts_with("node"); });
}

/// @brief 采集网络接口信息（operstate、MAC 地址、速率、device 符号链接）
/// @param raw 目标 RawStore
/// @param now 采集时间点
void read_sysfs_net(RawStore& raw, std::chrono::system_clock::time_point now)
{
    read_sysfs_dir("/sys/class/net",
                   [&](const auto& /*name*/, const auto& iface_path)
                   {
                       // 依次读取链路状态、MAC 地址、速率
                       for(auto sub : {"operstate", "address", "speed"})
                       {
                           auto content = read_file(iface_path + "/" + sub);
                           add_record(raw, RawSource::SysfsNet, iface_path + "/" + sub, content,
                                      now);
                       }

                       // 读取 device 符号链接，获取底层设备名
                       std::error_code ec;
                       auto device_link = fs::read_symlink(iface_path + "/device", ec);
                       if(!ec)
                       {
                           add_record(raw, RawSource::SysfsNet, iface_path + "/device",
                                      device_link.filename().string(), now);
                       }
                   });
}

/// @brief 采集 PCI 设备信息（vendor、device、class、numa_node）
/// @param raw 目标 RawStore
/// @param now 采集时间点
void read_sysfs_pci(RawStore& raw, std::chrono::system_clock::time_point now)
{
    read_sysfs_dir("/sys/bus/pci/devices",
                   [&](const auto& /*name*/, const auto& dev_path)
                   {
                       // 厂商 ID、设备 ID、设备类码
                       for(auto sub : {"vendor", "device", "class"})
                       {
                           auto content = read_file(dev_path + "/" + sub);
                           add_record(raw, RawSource::SysfsPci, dev_path + "/" + sub, content, now);
                       }

                       // PCI 设备所属 NUMA 节点
                       auto numa = read_file(dev_path + "/numa_node");
                       add_record(raw, RawSource::SysfsPci, dev_path + "/numa_node", numa, now);
                   });
}

/// @brief 采集块设备信息（容量扇区数、型号、device 符号链接）
/// @param raw 目标 RawStore
/// @param now 采集时间点
void read_sysfs_block(RawStore& raw, std::chrono::system_clock::time_point now)
{
    read_sysfs_dir("/sys/block",
                   [&](const auto& /*name*/, const auto& dev_path)
                   {
                       // 设备总扇区数（每扇区 512 字节）
                       auto size = read_file(dev_path + "/size");
                       add_record(raw, RawSource::SysfsBlock, dev_path + "/size", size, now);

                       // 设备型号字符串
                       auto model = read_file(dev_path + "/device/model");
                       add_record(raw, RawSource::SysfsBlock, dev_path + "/device/model", model,
                                  now);

                       // 读取 device 符号链接，获取底层总线设备名
                       std::error_code ec;
                       auto device_link = fs::read_symlink(dev_path + "/device", ec);
                       if(!ec)
                       {
                           add_record(raw, RawSource::SysfsBlock, dev_path + "/device",
                                      device_link.filename().string(), now);
                       }
                   });
}

} // namespace

/// @brief 采集 /sys 文件系统的原始数据
/// @param raw 输出的 RawStore，采集结果会追加到其中
/// @param spec 采集规格，决定哪些类别需要采集
void read_sysfs(RawStore& raw, const CollectSpec& spec)
{
    const auto now = std::chrono::system_clock::now();

    // CPU 或拓扑需要时采集 CPU 拓扑
    if(spec.collect_cpu() || spec.collect_topology())
    {
        read_sysfs_cpu(raw, now);
    }

    // 内存或拓扑需要时采集 NUMA 节点
    if(spec.collect_memory() || spec.collect_topology())
    {
        read_sysfs_numa(raw, now);
    }

    // 网络需要时采集网络接口
    if(spec.collect_network())
    {
        read_sysfs_net(raw, now);
    }

    // PCI 或拓扑需要时采集 PCI 设备
    if(spec.collect_pci() || spec.collect_topology())
    {
        read_sysfs_pci(raw, now);
    }

    // 存储需要时采集块设备
    if(spec.collect_storage())
    {
        read_sysfs_block(raw, now);
    }
}

} // namespace sysal::detail
