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

bool is_cpu_dir(const std::string& name)
{
    if(!name.starts_with("cpu"))
    {
        return false;
    }
    auto num_part = std::string_view(name).substr(3);
    if(num_part.empty())
    {
        return false;
    }
    return std::ranges::all_of(num_part, [](unsigned char c) { return std::isdigit(c); });
}

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
            continue;
        }
        read_entry(name, entry.path().string());
    }
}

void read_sysfs_cpu(RawStore& raw, std::chrono::system_clock::time_point now)
{
    const std::string base = "/sys/devices/system/cpu";

    auto online = read_file(base + "/online");
    add_record(raw, RawSource::SysfsCpu, base + "/online", online, now);

    read_sysfs_dir(
        base,
        [&](const auto& /*name*/, const auto& cpu_path)
        {
            auto pkg_id = read_file(cpu_path + "/topology/physical_package_id");
            add_record(raw, RawSource::SysfsCpu, cpu_path + "/topology/physical_package_id", pkg_id,
                       now);

            auto core_id = read_file(cpu_path + "/topology/core_id");
            add_record(raw, RawSource::SysfsCpu, cpu_path + "/topology/core_id", core_id, now);

            auto thread_siblings = read_file(cpu_path + "/topology/thread_siblings_list");
            add_record(raw, RawSource::SysfsCpu, cpu_path + "/topology/thread_siblings_list",
                       thread_siblings, now);

            auto core_siblings = read_file(cpu_path + "/topology/core_siblings_list");
            add_record(raw, RawSource::SysfsCpu, cpu_path + "/topology/core_siblings_list",
                       core_siblings, now);

            auto numa_node = read_file(cpu_path + "/numa_node");
            add_record(raw, RawSource::SysfsCpu, cpu_path + "/numa_node", numa_node, now);
        },
        is_cpu_dir);
}

void read_sysfs_numa(RawStore& raw, std::chrono::system_clock::time_point now)
{
    read_sysfs_dir(
        "/sys/devices/system/node",
        [&](const auto& /*name*/, const auto& node_path)
        {
            auto meminfo = read_file(node_path + "/meminfo");
            add_record(raw, RawSource::SysfsNuma, node_path + "/meminfo", meminfo, now);

            auto cpulist = read_file(node_path + "/cpulist");
            add_record(raw, RawSource::SysfsNuma, node_path + "/cpulist", cpulist, now);
        },
        [](const std::string& name) { return name.starts_with("node"); });
}

void read_sysfs_net(RawStore& raw, std::chrono::system_clock::time_point now)
{
    read_sysfs_dir("/sys/class/net",
                   [&](const auto& /*name*/, const auto& iface_path)
                   {
                       for(auto sub : {"operstate", "address", "speed"})
                       {
                           auto content = read_file(iface_path + "/" + sub);
                           add_record(raw, RawSource::SysfsNet, iface_path + "/" + sub, content,
                                      now);
                       }

                       std::error_code ec;
                       auto device_link = fs::read_symlink(iface_path + "/device", ec);
                       if(!ec)
                       {
                           add_record(raw, RawSource::SysfsNet, iface_path + "/device",
                                      device_link.filename().string(), now);
                       }
                   });
}

void read_sysfs_pci(RawStore& raw, std::chrono::system_clock::time_point now)
{
    read_sysfs_dir("/sys/bus/pci/devices",
                   [&](const auto& /*name*/, const auto& dev_path)
                   {
                       for(auto sub : {"vendor", "device", "class"})
                       {
                           auto content = read_file(dev_path + "/" + sub);
                           add_record(raw, RawSource::SysfsPci, dev_path + "/" + sub, content, now);
                       }

                       auto numa = read_file(dev_path + "/numa_node");
                       add_record(raw, RawSource::SysfsPci, dev_path + "/numa_node", numa, now);
                   });
}

void read_sysfs_block(RawStore& raw, std::chrono::system_clock::time_point now)
{
    read_sysfs_dir("/sys/block",
                   [&](const auto& /*name*/, const auto& dev_path)
                   {
                       auto size = read_file(dev_path + "/size");
                       add_record(raw, RawSource::SysfsBlock, dev_path + "/size", size, now);

                       auto model = read_file(dev_path + "/device/model");
                       add_record(raw, RawSource::SysfsBlock, dev_path + "/device/model", model,
                                  now);

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

void read_sysfs(RawStore& raw, const CollectSpec& spec)
{
    const auto now = std::chrono::system_clock::now();

    if(spec.collect_cpu() || spec.collect_topology())
    {
        read_sysfs_cpu(raw, now);
    }

    if(spec.collect_memory() || spec.collect_topology())
    {
        read_sysfs_numa(raw, now);
    }

    if(spec.collect_network())
    {
        read_sysfs_net(raw, now);
    }

    if(spec.collect_pci() || spec.collect_topology())
    {
        read_sysfs_pci(raw, now);
    }

    if(spec.collect_storage())
    {
        read_sysfs_block(raw, now);
    }
}

} // namespace sysal::detail
