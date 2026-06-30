/// @file enums.hpp
/// @brief sysal 公共枚举定义
/// @details 集中定义系统信息采集过程中使用的所有强类型枚举，涵盖架构、
///          接口状态、存储类型、加速器类型、ISA 扩展、原始数据来源、
///          采集状态、诊断严重级别、虚拟化类型、cgroup 版本与容器类型。

#pragma once

namespace sysal
{

/// @brief CPU 架构类型
enum class Architecture
{
    X86_64,  ///< x86-64 架构
    AArch64, ///< ARM 64 位架构
    Riscv64, ///< RISC-V 64 位架构
    Other,   ///< 其他或未知架构
};

/// @brief 网络接口链路状态
enum class InterfaceState
{
    Up,      ///< 接口已启用且链路正常
    Down,    ///< 接口已关闭或链路断开
    Unknown, ///< 无法确定链路状态
};

/// @brief 存储设备类型
enum class StorageKind
{
    Nvme,  ///< NVMe 设备
    Sata,  ///< SATA 设备
    Sas,   ///< SAS 设备
    Other, ///< 其他或未知类型
};

/// @brief 加速器设备类型
enum class AcceleratorKind
{
    Gpu,   ///< GPU 加速器
    Npu,   ///< 神经网络处理器（NPU）
    Fpga,  ///< 现场可编程门阵列（FPGA）
    Other, ///< 其他或未知类型
};

/// @brief CPU 指令集架构扩展
enum class IsaExtension
{
    Sse42,    ///< SSE4.2
    Avx,      ///< AVX
    Avx2,     ///< AVX2
    Avx512f,  ///< AVX-512 Foundation
    Avx512cd, ///< AVX-512 Conflict Detection
    Avx512bw, ///< AVX-512 Byte/Word
    Avx512dq, ///< AVX-512 Doubleword/Quadword
    Avx512vl, ///< AVX-512 Vector Length Extensions
    Neon,     ///< ARM NEON
    Sve,      ///< ARM SVE
    Sve2,     ///< ARM SVE2
    AmxInt8,  ///< Intel AMX Int8
};

/// @brief 原始数据来源类型
/// @details 标识每条原始记录来自哪个系统数据源，用于冲突解决排序与回放。
enum class RawSource
{
    // Linux (v0.0.1)
    ProcCpuInfo,    ///< /proc/cpuinfo
    ProcMemInfo,    ///< /proc/meminfo
    ProcVersion,    ///< /proc/version
    ProcUname,      ///< uname 系统调用
    SysfsCpu,       ///< /sys/devices/system/cpu
    SysfsNet,       ///< /sys/class/net
    SysfsPci,       ///< /sys/bus/pci
    SysfsNuma,      ///< /sys/devices/system/node
    SysfsClassNet,  ///< /sys/class/net
    SysfsBlock,     ///< /sys/block
    ProcSelfCgroup, ///< /proc/self/cgroup
    ProcSelfStatus, ///< /proc/self/status
    ProcNetInet,    ///< /proc/net
    Lspci,          ///< lspci 命令输出
    Lsblk,          ///< lsblk 命令输出
    HwlocXml,       ///< hwloc XML 拓扑导出
    HwinfoOutput,   ///< hwinfo 命令输出
    Nvml,           ///< NVML 库
    NvidiaSmi,      ///< nvidia-smi 命令输出
    Ibverbs,        ///< libibverbs 设备查询
                    // Windows / macOS — future
};

/// @brief 单项数据采集状态
enum class CollectStatus
{
    Success,     ///< 采集成功
    Partial,     ///< 部分采集成功
    Failed,      ///< 采集失败
    NotCollected ///< 未采集该项
};

/// @brief 诊断记录的严重级别
enum class Severity
{
    Info,    ///< 提示信息
    Warning, ///< 警告
    Error,   ///< 错误
};

/// @brief 虚拟化类型
enum class VirtualizationKind
{
    None,       ///< 未运行在虚拟化/容器环境中
    Kvm,        ///< KVM 虚拟机
    Xen,        ///< Xen 虚拟机
    HyperV,     ///< Hyper-V 虚拟机
    Vmware,     ///< VMware 虚拟机
    Docker,     ///< Docker 容器
    Podman,     ///< Podman 容器
    Lxc,        ///< LXC 容器
    Kubernetes, ///< Kubernetes Pod
    Other,      ///< 其他或未知虚拟化环境
};

/// @brief cgroup 版本
enum class CgroupVersion
{
    V1,  ///< cgroup v1
    V2,  ///< cgroup v2
    None ///< 未使用 cgroup
};

/// @brief 容器类型
enum class ContainerKind
{
    Docker,     ///< Docker 容器
    Podman,     ///< Podman 容器
    Lxc,        ///< LXC 容器
    Kubernetes, ///< Kubernetes Pod
    Other,      ///< 其他或未知容器
};

} // namespace sysal
