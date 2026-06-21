#pragma once

namespace sysal
{

enum class Architecture
{
    X86_64,
    AArch64,
    Riscv64,
    Other,
};

enum class InterfaceState
{
    Up,
    Down,
    Unknown,
};

enum class StorageKind
{
    Nvme,
    Sata,
    Sas,
    Other,
};

enum class AcceleratorKind
{
    Gpu,
    Npu,
    Fpga,
    Other,
};

enum class IsaExtension
{
    Sse42,
    Avx,
    Avx2,
    Avx512f,
    Avx512cd,
    Avx512bw,
    Avx512dq,
    Avx512vl,
    Neon,
    Sve,
    Sve2,
    AmxInt8,
};

enum class RawSource
{
    // Linux (v0.0.1)
    ProcCpuInfo,
    ProcMemInfo,
    ProcVersion,
    ProcUname,
    SysfsCpu,
    SysfsNet,
    SysfsPci,
    SysfsNuma,
    SysfsClassNet,
    SysfsBlock,
    ProcSelfCgroup,
    ProcSelfStatus,
    ProcNetInet,
    Lspci,
    Lsblk,
    HwlocXml,
    HwinfoOutput,
    Nvml,
    NvidiaSmi,
    Ibverbs,
    // Windows / macOS — future
};

enum class CollectStatus
{
    Success,
    Partial,
    Failed,
    NotCollected,
};

enum class Severity
{
    Info,
    Warning,
    Error,
};

enum class VirtualizationKind
{
    None,
    Kvm,
    Xen,
    HyperV,
    Vmware,
    Docker,
    Podman,
    Lxc,
    Kubernetes,
    Other,
};

enum class CgroupVersion
{
    V1,
    V2,
    None,
};

enum class ContainerKind
{
    Docker,
    Podman,
    Lxc,
    Kubernetes,
    Other,
};

} // namespace sysal
