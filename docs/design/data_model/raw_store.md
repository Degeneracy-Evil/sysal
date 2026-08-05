# RawStore

存储从系统采集的原始证据。

```cpp
enum class RawSource
{
    // Linux procfs
    ProcCpuInfo,
    ProcMemInfo,
    ProcVersion,
    ProcSelfCgroup,
    ProcSelfStatus,
    ProcOneCgroup,
    // Linux sysfs
    SysfsCpu,
    SysfsNuma,
    SysfsNet,
    SysfsPci,
    SysfsBlock,
    SysfsDmi,
    // Linux 文件 / 命令
    EtcOsRelease,
    RootDockerenv,
    Uname,
    Lspci,
    NvidiaSmi,
    Nvcc,
    Lsblk,
    // 环境变量
    Environment,
    // 外部库后端（未来支持）
    Nvml,
    Ibverbs,
    HwinfoOutput,
    // 新增来源（追加在末尾以保持枚举值稳定性）
    ProcHostname,   ///< /proc/sys/kernel/hostname
    IfAddrs,        ///< getifaddrs() 网络接口地址
    DfTh,           ///< df -Th 文件系统挂载信息
    Udevadm,        ///< udevadm info -e 硬件数据库
    SysfsEdac,      ///< /sys/devices/system/edac 内存 DIMM 信息
    SysHypervisor,  ///< /sys/hypervisor/type
    CompilerVersion, ///< 编译器 --version 命令输出
    CompilerPath,    ///< 编译器通用命令查找路径（command -v）
    CompilerTarget,  ///< 编译器 -dumpmachine 目标架构输出
    MpiVersion,      ///< MPI 实现 --version 命令输出
    MpiPath,         ///< MPI 可执行文件路径（command -v）
    IbverbsVersion,  ///< libibverbs 版本（pkg-config）
    IbverbsLibdir,   ///< libibverbs 库目录（pkg-config）
    UcxVersion,      ///< UCX 版本（pkg-config）
    NvccPath,        ///< nvcc 可执行文件路径
    CudaHome,        ///< CUDA_HOME 环境变量
    SysfsThermal,    ///< /sys/class/thermal 温度传感器
};

enum class CollectStatus
{
    Success,
    Partial,
    Failed,
    NotCollected,
};

struct RawRecord
{
    RawSource source;
    std::string path_or_command;           // 次级键
    std::string payload;
    CollectStatus status;
    std::chrono::system_clock::time_point collected_at;
};

struct RawStore
{
    std::vector<RawRecord> records;

    std::vector<const RawRecord*> get_all(RawSource source) const;
    std::vector<const RawRecord*> get(RawSource source,
                                      std::string_view path_or_command) const;
    bool has(RawSource source) const;
    bool has_success(RawSource source) const; ///< 检查指定来源是否有成功采集的记录
    std::size_t count(RawSource source) const;
};
```

一个 `RawSource` 可能对应多条记录（例如 `SysfsCpu` 下有许多 sysfs 文件）。
`path_or_command` 作为次级键，用于细粒度访问。

`RawStore` 在 `System` 中是可选的。通过在 `System::collect()` 时
将 `Collect::Raw` 加入请求的 `Collect` 位掩码来启用采集；未设置该标志时
`System::raw` 为 `std::nullopt`。
