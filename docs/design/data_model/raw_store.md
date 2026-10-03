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
    // 外部库后端（NVML 已支持；其他来源按实际后端演进）
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
    CgroupMountInfo, ///< cgroup 挂载点与层级根
    CgroupFile,      ///< cgroup v1/v2 配额文件
    SysfsDrm,       ///< GPU sysfs 库存
    NvidiaMigList,  ///< nvidia-smi -L
    RocmVersion,    ///< ROCm 版本 / HIP 路径命令与文件
    PackageLibrary, ///< pkg-config 版本 / libdir
    AcceleratorRuntime, ///< CUDA Driver / HIP / Level Zero 枚举
    StorageMountInfo, ///< /proc/self/mountinfo
    Ethtool,         ///< 网卡只读能力查询
    ProcNetVlan,     ///< /proc/net/vlan/config
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
    std::optional<ReadFailure> failure{};
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

## 读取结果与来源观察

`ReadFailure` 区分 NotPresent、PermissionDenied、Unsupported、IoError、
ToolUnavailable、TimedOut、NotProvided。不存在分类证据时保持缺失，不推断原因。
读取失败保留原来源与路径，不将失败值当作有效硬件配置。

`SnapshotMeta.observations` 保留普通硬件采集的 domain、source、origin、status、failure，
不包含 payload，因此无需请求 `Collect::Raw` 也能查看采集来源。
它记录的是来源查询结果，并非每个字段的完整追踪：Success 不保证来源提供全部字段；
DMI 空值及占位文本记为 Partial / NotProvided。旧 JSON 没有这些新字段时使用默认值。
Sysal 枚举的新值仅追加，保留已有序列化编号。
