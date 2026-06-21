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

void read_proc_file(RawStore& raw, RawSource source, std::string_view path,
                    std::chrono::system_clock::time_point now)
{
    auto content = read_file(std::string(path));
    add_record(raw, source, std::string(path), content, now);
}

void read_command_output(RawStore& raw, RawSource source, std::string_view command,
                         std::chrono::system_clock::time_point now)
{
    auto content = read_command(std::string(command));
    add_record(raw, source, std::string(command), content, now);
}

void read_uname(RawStore& raw, std::chrono::system_clock::time_point now)
{
    struct utsname buf
    {
    };
    if(uname(&buf) == 0)
    {
        std::string payload = std::string(buf.sysname) + " " + std::string(buf.nodename) + " " +
                              std::string(buf.release) + " " + std::string(buf.version) + " " +
                              std::string(buf.machine);
        add_record(raw, RawSource::ProcUname, "uname", payload, now);
    }
    else
    {
        add_record(raw, RawSource::ProcUname, "uname", std::nullopt, now);
    }
}

} // namespace

void read_procfs(RawStore& raw, const CollectSpec& spec)
{
    const auto now = std::chrono::system_clock::now();

    if(spec.collect_cpu())
    {
        read_proc_file(raw, RawSource::ProcCpuInfo, "/proc/cpuinfo", now);
    }

    if(spec.collect_memory())
    {
        read_proc_file(raw, RawSource::ProcMemInfo, "/proc/meminfo", now);
    }

    if(spec.collect_platform())
    {
        read_proc_file(raw, RawSource::ProcVersion, "/proc/version", now);
        read_proc_file(raw, RawSource::ProcUname, "/etc/os-release", now);
        read_uname(raw, now);
    }

    if(spec.collect_execution_context())
    {
        read_proc_file(raw, RawSource::ProcSelfCgroup, "/proc/self/cgroup", now);
        read_proc_file(raw, RawSource::ProcSelfStatus, "/proc/self/status", now);
    }

    if(spec.collect_accelerators() || spec.collect_software_stack())
    {
        read_command_output(
            raw, RawSource::NvidiaSmi,
            "nvidia-smi --query-gpu=index,name,memory.total,pci.bus_id,driver_version "
            "--format=csv,noheader,nounits",
            now);
    }

    if(spec.collect_software_stack())
    {
        read_proc_file(raw, RawSource::NvidiaSmi, "/proc/driver/nvidia/version", now);
        read_command_output(raw, RawSource::NvidiaSmi, "nvcc --version", now);
    }
}

} // namespace sysal::detail
