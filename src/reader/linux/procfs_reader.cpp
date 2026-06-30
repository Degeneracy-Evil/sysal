/// @file procfs_reader.cpp
/// @brief procfs 采集器实现
/// @details 实现 read_procfs，按 CollectSpec 的开关分别采集
///          /proc/cpuinfo、/proc/meminfo、/proc/version、/etc/os-release、
///          uname、/proc/self/cgroup、/proc/self/status，以及 nvidia-smi、nvcc 等命令输出。

#include "procfs_reader.hpp"
#include "file_utils.hpp"

#include <sys/utsname.h>

#include <chrono>
#include <string>
#include <string_view>

namespace sysal::detail
{

namespace
{

/// @brief 读取单个 /proc 文件并追加到 RawStore
/// @param raw 目标 RawStore
/// @param source 数据来源标识
/// @param path /proc 文件路径
/// @param now 采集时间点
void read_proc_file(RawStore& raw, RawSource source, std::string_view path,
                    std::chrono::system_clock::time_point now)
{
    auto content = read_file(std::string(path));
    add_record(raw, source, std::string(path), content, now);
}

/// @brief 执行外部命令并将输出追加到 RawStore
/// @param raw 目标 RawStore
/// @param source 数据来源标识
/// @param command 要执行的 shell 命令
/// @param now 采集时间点
void read_command_output(RawStore& raw, RawSource source, std::string_view command,
                         std::chrono::system_clock::time_point now)
{
    auto content = read_command(std::string(command));
    add_record(raw, source, std::string(command), content, now);
}

/// @brief 调用 uname(2) 获取内核信息并追加到 RawStore
/// @param raw 目标 RawStore
/// @param now 采集时间点
void read_uname(RawStore& raw, std::chrono::system_clock::time_point now)
{
    struct utsname buf
    {
    };
    if(uname(&buf) == 0)
    {
        // 拼接 sysname nodename release version machine 五个字段
        std::string payload = std::string(buf.sysname) + " " + std::string(buf.nodename) + " " +
                              std::string(buf.release) + " " + std::string(buf.version) + " " +
                              std::string(buf.machine);
        add_record(raw, RawSource::ProcUname, "uname", payload, now);
    }
    else
    {
        // uname 调用失败，记录失败状态
        add_record(raw, RawSource::ProcUname, "uname", std::nullopt, now);
    }
}

} // namespace

/// @brief 采集 /proc 及相关命令的原始数据
/// @param raw 输出的 RawStore，采集结果会追加到其中
/// @param spec 采集规格，决定哪些类别需要采集
void read_procfs(RawStore& raw, const CollectSpec& spec)
{
    const auto now = std::chrono::system_clock::now();

    // CPU 信息：/proc/cpuinfo
    if(spec.collect_cpu())
    {
        read_proc_file(raw, RawSource::ProcCpuInfo, "/proc/cpuinfo", now);
    }

    // 内存信息：/proc/meminfo
    if(spec.collect_memory())
    {
        read_proc_file(raw, RawSource::ProcMemInfo, "/proc/meminfo", now);
    }

    // 平台信息：内核版本、发行版、uname
    if(spec.collect_platform())
    {
        read_proc_file(raw, RawSource::ProcVersion, "/proc/version", now);
        read_proc_file(raw, RawSource::ProcUname, "/etc/os-release", now);
        read_uname(raw, now);
    }

    // 执行上下文：cgroup 与进程状态
    if(spec.collect_execution_context())
    {
        read_proc_file(raw, RawSource::ProcSelfCgroup, "/proc/self/cgroup", now);
        read_proc_file(raw, RawSource::ProcSelfStatus, "/proc/self/status", now);
    }

    // 加速器或软件栈：查询 NVIDIA GPU 基本信息
    if(spec.collect_accelerators() || spec.collect_software_stack())
    {
        read_command_output(
            raw, RawSource::NvidiaSmi,
            "nvidia-smi --query-gpu=index,name,memory.total,pci.bus_id,driver_version "
            "--format=csv,noheader,nounits",
            now);
    }

    // 软件栈：NVIDIA 驱动版本文件与 nvcc 编译器版本
    if(spec.collect_software_stack())
    {
        read_proc_file(raw, RawSource::NvidiaSmi, "/proc/driver/nvidia/version", now);
        read_command_output(raw, RawSource::NvidiaSmi, "nvcc --version", now);
    }
}

} // namespace sysal::detail
