/// @file enums.hpp
/// @brief 枚举类型定义
/// @details 定义 sysal 中所有作用域枚举（enum class），覆盖架构、接口状态、
///          存储类型、加速器类型、ISA 扩展、原始数据来源、采集状态等。

#pragma once

#include <cstdint>

namespace sysal
{

    /// @brief CPU 架构
    enum class Arch
    {
        X86_64,
        AArch64, ///< ARM 64
        Riscv64, ///< RISC-V 64
        Other    ///< 其他或未知
    };

    /// @brief 网络接口链路状态
    enum class InterfaceState
    {
        Up,   ///< 链路已连接
        Down, ///< 链路已断开
        Unknown
    };

    /// @brief 存储设备类型
    enum class StorageKind
    {
        Nvme, ///< NVMe SSD
        Ssd,  ///< SATA/SAS SSD（rotational=0）
        Hdd,  ///< 机械硬盘（rotational=1）
        Other ///< 其他或未知
    };

    /// @brief 加速器设备类型
    enum class AcceleratorKind
    {
        Gpu,
        Npu,  ///< 神经网络处理器
        Fpga, ///< 现场可编程门阵列
        Other ///< 其他或未知
    };

    /// @brief CPU ISA 扩展
    enum class IsaExtension
    {
        Sse,
        Sse2,
        Sse3,
        Ssse3,
        Sse41,
        Sse42,
        Avx,
        Avx2,
        Avx512f,  ///< AVX-512 Foundation
        Avx512cd, ///< AVX-512 Conflict Detection
        Avx512bw, ///< AVX-512 Byte/Word
        Avx512dq, ///< AVX-512 Doubleword/Quadword
        Avx512vl, ///< AVX-512 Vector Length
        Aes,      ///< AES 指令集
        Fma,
        F16c,
        Pclmulqdq
    };

    /// @brief 原始数据来源
    enum class RawSource
    {
        // Linux procfs
        ProcCpuInfo,    ///< /proc/cpuinfo
        ProcMemInfo,    ///< /proc/meminfo
        ProcVersion,    ///< /proc/version
        ProcSelfCgroup, ///< /proc/self/cgroup
        ProcSelfStatus, ///< /proc/self/status
        ProcOneCgroup,  ///< /proc/1/cgroup

        // Linux sysfs
        SysfsCpu,   ///< /sys/devices/system/cpu
        SysfsNuma,  ///< /sys/devices/system/node
        SysfsNet,   ///< /sys/class/net
        SysfsPci,   ///< /sys/bus/pci/devices
        SysfsBlock, ///< /sys/block
        SysfsDmi,   ///< /sys/class/dmi/id

        // Linux 文件 / 命令
        EtcOsRelease,  ///< /etc/os-release
        RootDockerenv, ///< /.dockerenv
        Uname,         ///< uname -m
        Lspci,         ///< lspci 命令输出
        NvidiaSmi,     ///< nvidia-smi 命令输出
        Nvcc,          ///< nvcc --version 命令输出
        Lsblk,         ///< lsblk 命令输出

        // 环境变量
        Environment, ///< 进程环境变量

        // 外部库后端（未来支持）
        Nvml,         ///< NVML 库
        Ibverbs,      ///< ibverbs 库
        HwinfoOutput, ///< hwinfo 命令输出

        // 新增来源（追加在末尾以保持枚举值稳定性）
        ProcHostname,    ///< /proc/sys/kernel/hostname
        IfAddrs,         ///< getifaddrs() 网络接口地址
        DfTh,            ///< df -Th 文件系统挂载信息
        Udevadm,         ///< udevadm info -e 硬件数据库
        SysfsEdac,       ///< /sys/devices/system/edac 内存 DIMM 信息
        SysHypervisor,   ///< /sys/hypervisor/type
        CompilerVersion, ///< 编译器 --version 命令输出
        CompilerPath,    ///< 编译器通用命令查找路径（command -v）
        CompilerTarget,  ///< 编译器 -dumpmachine 目标架构输出
        MpiVersion,      ///< MPI 实现 --version 命令输出
        MpiPath,         ///< MPI 可执行文件路径（command -v）
    };

    /// @brief 单个原始记录的采集状态
    enum class CollectStatus
    {
        Success,
        Partial,
        Failed,
        NotCollected
    };

    /// @brief 虚拟化类型
    enum class VirtualizationKind
    {
        None, ///< 物理机
        Kvm,
        Xen,
        Vmware,
        Qemu,
        HyperV, ///< Hyper-V
        VirtualBox,
        Parallels,
        Other
    };

    /// @brief cgroup 版本
    enum class CgroupVersion
    {
        V1,
        V2
    };

    /// @brief 容器类型
    enum class ContainerKind
    {
        Docker,
        Podman,
        Lxc,        ///< LXC
        Kubernetes, ///< Kubernetes
        Other
    };

} // namespace sysal
