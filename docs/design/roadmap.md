# 路线图

## v0.0.1 实现范围

必需项：

```txt
Public API:
    System::collect(Collect) -> System
    System::refresh()
    Collect bitmask enum + basic / full presets
    不需要全局 init()，后端初始化在 collect() 内部自动完成

Core model:
    System (持有 SystemInfo / SnapshotMeta / warnings / raw)
    SystemInfo (扁平结构：platform / cpu / memory / accelerators
                / network / storage / pci / software / execution)
    PlatformInfo
    CpuSubsystem / MemorySubsystem / AcceleratorSubsystem
    NetworkSubsystem / StorageSubsystem / PciSubsystem
    SoftwareStackInfo
    ExecutionContextInfo (with visibility indexes)
    warnings (std::vector<std::string>)
    RawStore (multi-record)

Internal pipeline:
    Reader → RawStore → Parser → ParseResult → Resolver → System

Linux support:
    procfs, sysfs, PCI, basic network, basic CPU and memory

Optional backend:
    NVML for NVIDIA GPU

Testing:
    collect_from_raw + load/save_raw_store for raw replay

Thread safety:
    无状态 collect，不可变 System 对象
```

建议的首批实现目标：

```txt
CPU:         architecture, packages, cores, logical CPUs (with parent IDs),
             ISA extensions, visible CPUs, per-device numa_node
Memory:      total, NUMA memory
Accelerator: NVIDIA GPU name, memory, PCI address, visibility, numa_node
Network:     name, state, speed, IP, PCI address, visibility
Software:    OS, kernel, NVIDIA driver, CUDA runtime
Execution:   env vars, cgroup/cpuset visibility, container detection
Raw:         optional raw records
warnings:    采集 / 解析 / 解决过程中的非致命问题
```

## v0.0.1 非目标

```txt
Performance scoring
Benchmark execution
Operator selection
Scheduling policy
Long-term monitoring
Daemon mode
Database storage
Web API
Complex plugin system
Full cross-platform support
Complete hardware inventory
拓扑信息（已有 hwloc 等成熟库）
全局 init() 函数（无此需求）
```

## 未来扩展

### 缓存（已内置）

v0.0.1 的 `System` 类已是对象持有模式：`System` 对象本身即该次采集结果的缓存。
调用方持有 `System` 对象即可反复读取类型化数据，无需重复采集。
`refresh()` 在已有对象上重新采集并替换内部状态，作为显式的缓存失效与重建入口。

无需额外的 `Collector` 类或 `collect_incremental()` 接口。

### 跨平台（v0.0.1 之后）

v0.0.1 仅支持 Linux。扩展路径：

1. `RawSource` 枚举增加平台特定的值（例如 `WmiCpu`、`SysctlHw`）。
2. Reader 位于 `src/reader/<platform>/` 下，由 xmake 在构建时选择。
3. 无需修改公共 API —— `System` 与平台无关。

### 拓扑信息（未来可选）

v0.0.1 不提供 NUMA / PCI 关系图等拓扑抽象，因为已有 hwloc 等成熟库可胜任。
设备级 `numa_node` 字段保留在 `PciDevice`、`AcceleratorDevice`、`CpuCore`、`LogicalCpu` 上，直接从 sysfs 读取。

未来如需封装更高层的拓扑关系，可作为独立模块引入，不污染核心数据模型。
