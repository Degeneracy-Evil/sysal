/// @file procfs.cpp
/// @brief Linux procfs 采集器实现
/// @details 从 /proc、/etc、外部命令及环境变量采集原始数据写入 RawStore。
///          使用 has(flags, Collect::Xxx) 决定是否采集各域。

#include "reader/linux/procfs.hpp"
#include "reader/linux/file_utils.hpp"

#include <arpa/inet.h>
#include <cstdlib>
#include <ifaddrs.h>
#include <net/if.h>
#include <string>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <unistd.h>

namespace sysal::reader
{

    namespace
    {

        /// @brief 采集单个 procfs 文件
        /// @param raw 原始证据存储
        /// @param source 原始数据来源
        /// @param path 文件路径
        void read_proc_file(RawStore &raw, RawSource source, const std::string &path)
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
        void read_cmd(RawStore &raw, RawSource source, const std::string &cmd)
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

        /// @brief 通过 uname() 系统调用采集架构与内核信息
        /// @param raw 原始证据存储
        /// @details 一次 uname() 调用填充 Uname 与 ProcVersion 两条记录：
        ///          Uname.payload = buf.machine；ProcVersion.payload = "release\nversion"。
        void read_uname(RawStore &raw)
        {
            struct utsname buf;
            if(uname(&buf) != 0)
            {
                add_record(raw, RawSource::Uname, "uname", "", CollectStatus::Failed);
                add_record(raw, RawSource::ProcVersion, "uname", "", CollectStatus::Failed);
                return;
            }
            add_record(raw, RawSource::Uname, "uname", buf.machine, CollectStatus::Success);
            std::string version_payload = std::string(buf.release) + "\n" + buf.version;
            add_record(raw, RawSource::ProcVersion, "uname", version_payload, CollectStatus::Success);
        }

        /// @brief 通过 gethostname() 系统调用采集主机名
        /// @param raw 原始证据存储
        void read_hostname(RawStore &raw)
        {
            char buf[256];
            if(gethostname(buf, sizeof(buf)) != 0)
            {
                add_record(raw, RawSource::ProcHostname, "gethostname", "", CollectStatus::Failed);
                return;
            }
            add_record(raw, RawSource::ProcHostname, "gethostname", buf, CollectStatus::Success);
        }

        /// @brief 采集环境变量
        /// @param raw 原始证据存储
        /// @param names 要采集的环境变量名列表
        void read_env_vars(RawStore &raw, const char *const names[], std::size_t count)
        {
            std::string payload;
            for(std::size_t i = 0; i < count; ++i)
            {
                const char *val = std::getenv(names[i]);
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

        /// @brief 采集网络接口 IP 地址（getifaddrs）
        /// @param raw 原始证据存储
        void read_ifaddrs(RawStore &raw)
        {
            struct ifaddrs *ifap = nullptr;
            if(getifaddrs(&ifap) != 0)
            {
                add_record(raw, RawSource::IfAddrs, "getifaddrs", "", CollectStatus::Failed);
                return;
            }

            std::string payload;
            char buf[INET6_ADDRSTRLEN];
            for(struct ifaddrs *p = ifap; p; p = p->ifa_next)
            {
                if(!p->ifa_addr)
                {
                    continue;
                }
                const char *ifname = p->ifa_name;
                auto family = p->ifa_addr->sa_family;
                if(family == AF_INET)
                {
                    auto *sa = reinterpret_cast<struct sockaddr_in *>(p->ifa_addr);
                    if(inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf)))
                    {
                        payload += ifname;
                        payload += ' ';
                        payload += buf;
                        payload += '\n';
                    }
                }
                else if(family == AF_INET6)
                {
                    auto *sa = reinterpret_cast<struct sockaddr_in6 *>(p->ifa_addr);
                    if(inet_ntop(AF_INET6, &sa->sin6_addr, buf, sizeof(buf)))
                    {
                        payload += ifname;
                        payload += ' ';
                        payload += buf;
                        payload += '\n';
                    }
                }
            }
            freeifaddrs(ifap);

            if(!payload.empty())
            {
                add_record(raw, RawSource::IfAddrs, "getifaddrs", payload, CollectStatus::Success);
            }
            else
            {
                add_record(raw, RawSource::IfAddrs, "getifaddrs", "", CollectStatus::NotCollected);
            }
        }

    } // namespace

    void read_procfs(RawStore &raw, Collect flags)
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
            read_uname(raw);
            read_proc_file(raw, RawSource::EtcOsRelease, "/etc/os-release");
            read_hostname(raw);

            // /.dockerenv 容器标记文件
            if(file_exists("/.dockerenv"))
            {
                auto content = read_file("/.dockerenv");
                add_record(raw, RawSource::RootDockerenv, "/.dockerenv", content.value_or(""), CollectStatus::Success);
            }
            else
            {
                add_record(raw, RawSource::RootDockerenv, "/.dockerenv", "", CollectStatus::NotCollected);
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
            read_cmd(raw, RawSource::Udevadm, "udevadm info -e");
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
            read_ifaddrs(raw);
        }

        // ---- Storage 域 ----
        if(has(flags, Collect::Storage))
        {
            read_cmd(raw, RawSource::DfTh, "df -Th");
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

            // 编译器探测：缺失的命令 read_cmd 静默记 Failed，不产生 warning
            static const char *const compilers[] = {"gcc", "g++", "clang", "clang++", "gfortran"};
            for(const char *cc : compilers)
            {
                read_cmd(raw, RawSource::CompilerVersion, std::string(cc) + " --version");
                read_cmd(raw, RawSource::CompilerPath, "command -v " + std::string(cc));
                read_cmd(raw, RawSource::CompilerTarget, std::string(cc) + " -dumpmachine");
            }

            // MPI 探测：OpenMPI/MPICH/MVAPICH2 均提供 mpirun 命令
            // 缺失的命令 read_cmd 静默记 Failed，不产生 warning
            read_cmd(raw, RawSource::MpiVersion, "mpirun --version");
            read_cmd(raw, RawSource::MpiPath, "command -v mpirun");
        }

        // ---- Execution 域 ----
        if(has(flags, Collect::Execution))
        {
            read_proc_file(raw, RawSource::ProcSelfCgroup, "/proc/self/cgroup");
            read_proc_file(raw, RawSource::ProcSelfStatus, "/proc/self/status");
            read_proc_file(raw, RawSource::ProcOneCgroup, "/proc/1/cgroup");

            // 采集关键环境变量
            static const char *const env_names[] = {
                "CUDA_VISIBLE_DEVICES", "HIP_VISIBLE_DEVICES", "ONEAPI_DEVICE_SELECTOR",  "OMP_NUM_THREADS",
                "MLU_VISIBLE_DEVICES",  "container",           "KUBERNETES_SERVICE_HOST",
            };
            read_env_vars(raw, env_names, sizeof(env_names) / sizeof(env_names[0]));
        }
    }

} // namespace sysal::reader
