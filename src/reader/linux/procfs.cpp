/// @file procfs.cpp
/// @brief Linux procfs 采集器实现
/// @details 从 /proc、/etc、外部命令及环境变量采集原始数据写入 RawStore。
///          使用 has(flags, Collect::Xxx) 决定是否采集各域。

#include "reader/linux/procfs.hpp"
#include "reader/linux/file_utils.hpp"

#include <cstdlib>
#include <string>

namespace sysal::reader
{

namespace
{

/// @brief 采集单个 procfs 文件
/// @param raw 原始证据存储
/// @param source 原始数据来源
/// @param path 文件路径
void read_proc_file(RawStore& raw, RawSource source, const std::string& path)
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

/// @brief 采集单个命令输出
/// @param raw 原始证据存储
/// @param source 原始数据来源
/// @param cmd 命令字符串
void read_cmd(RawStore& raw, RawSource source, const std::string& cmd)
{
    auto output = read_command(cmd);
    if(output)
    {
        add_record(raw, source, cmd, *output, CollectStatus::Success);
    }
    else
    {
        add_record(raw, source, cmd, "", CollectStatus::Failed);
    }
}

/// @brief 采集环境变量
/// @param raw 原始证据存储
/// @param names 要采集的环境变量名列表
void read_env_vars(RawStore& raw, const char* const names[], std::size_t count)
{
    std::string payload;
    for(std::size_t i = 0; i < count; ++i)
    {
        const char* val = std::getenv(names[i]);
        if(val)
        {
            if(!payload.empty())
            {
                payload += '\n';
            }
            payload += names[i];
            payload += '=';
            payload += val;
        }
    }
    if(!payload.empty())
    {
        add_record(raw, RawSource::Environment, "env", payload, CollectStatus::Success);
    }
    else
    {
        add_record(raw, RawSource::Environment, "env", "", CollectStatus::NotCollected);
    }
}

} // namespace

void read_procfs(RawStore& raw, Collect flags)
{
    // 三个域存在跨域依赖，无法扁平化为表驱动分派：
    //   Cpu → Platform（共享 ProcCpuInfo）
    //   Pci → Network（共享 Lspci）
    //   Software → Accelerator（共享 NvidiaSmi/Nvcc）
    // 依赖域在自身请求而依赖域未请求时，须单独补充采集共享数据。

    // ---- Platform 域 ----
    if(has(flags, Collect::Platform))
    {
        read_proc_file(raw, RawSource::ProcCpuInfo, "/proc/cpuinfo");
        read_proc_file(raw, RawSource::ProcVersion, "/proc/version");
        read_proc_file(raw, RawSource::EtcOsRelease, "/etc/os-release");
        read_cmd(raw, RawSource::Uname, "uname -m");
        read_proc_file(raw, RawSource::ProcHostname, "/proc/sys/kernel/hostname");

        // /.dockerenv 容器标记文件
        if(file_exists("/.dockerenv"))
        {
            auto content = read_file("/.dockerenv");
            add_record(raw, RawSource::RootDockerenv, "/.dockerenv", content.value_or(""),
                       CollectStatus::Success);
        }
        else
        {
            add_record(raw, RawSource::RootDockerenv, "/.dockerenv", "",
                       CollectStatus::NotCollected);
        }
    }

    // ---- Cpu 域 ----
    if(has(flags, Collect::Cpu))
    {
        // ProcCpuInfo 已在 Platform 域采集，此处仅当 Platform 未采集时补充
        if(!has(flags, Collect::Platform))
        {
            read_proc_file(raw, RawSource::ProcCpuInfo, "/proc/cpuinfo");
        }
    }

    // ---- Memory 域 ----
    if(has(flags, Collect::Memory))
    {
        read_proc_file(raw, RawSource::ProcMemInfo, "/proc/meminfo");
    }

    // ---- Accelerator 域 ----
    if(has(flags, Collect::Accelerator))
    {
        read_cmd(raw, RawSource::NvidiaSmi,
                 "nvidia-smi --query-gpu=index,name,memory.total,"
                 "pci.bus_id,driver_version --format=csv,noheader");
        read_cmd(raw, RawSource::Nvcc, "nvcc --version");
    }

    // ---- Network 域 ----
    if(has(flags, Collect::Network))
    {
        read_cmd(raw, RawSource::Lspci, "lspci -nn");
    }

    // ---- Storage 域 ----
    if(has(flags, Collect::Storage))
    {
        read_cmd(raw, RawSource::Lsblk, "lsblk -b -n -o NAME,SIZE,TYPE,MOUNTPOINT");
    }

    // ---- Pci 域 ----
    if(has(flags, Collect::Pci))
    {
        // Lspci 已在 Network 域采集，此处仅当 Network 未采集时补充
        if(!has(flags, Collect::Network))
        {
            read_cmd(raw, RawSource::Lspci, "lspci -nn");
        }
    }

    // ---- Software 域 ----
    if(has(flags, Collect::Software))
    {
        // NvidiaSmi / Nvcc 已在 Accelerator 域采集，此处仅当 Accelerator 未采集时补充
        if(!has(flags, Collect::Accelerator))
        {
            read_cmd(raw, RawSource::NvidiaSmi,
                     "nvidia-smi --query-gpu=index,name,memory.total,"
                     "pci.bus_id,driver_version --format=csv,noheader");
            read_cmd(raw, RawSource::Nvcc, "nvcc --version");
        }
    }

    // ---- Execution 域 ----
    if(has(flags, Collect::Execution))
    {
        read_proc_file(raw, RawSource::ProcSelfCgroup, "/proc/self/cgroup");
        read_proc_file(raw, RawSource::ProcSelfStatus, "/proc/self/status");
        read_proc_file(raw, RawSource::ProcOneCgroup, "/proc/1/cgroup");

        // 采集关键环境变量
        static const char* const env_names[] = {
            "CUDA_VISIBLE_DEVICES",    "HIP_VISIBLE_DEVICES", "ONEAPI_DEVICE_SELECTOR",
            "OMP_NUM_THREADS",         "MLU_VISIBLE_DEVICES", "container",
            "KUBERNETES_SERVICE_HOST",
        };
        read_env_vars(raw, env_names, sizeof(env_names) / sizeof(env_names[0]));
    }
}

} // namespace sysal::reader
