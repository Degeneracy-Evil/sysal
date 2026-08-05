# Cpu

CPU 子系统描述处理器的拓扑与基本属性。sysal 通过父 ID 字段表达
`package → core → 逻辑 CPU` 的三层层次关系，避免使用嵌套结构，使各层
可以独立遍历与查询。

## 强类型标识符

CPU 拓扑中三类对象各自拥有独立的强类型 ID，防止相互误用。

```cpp
// CPU 物理封装（socket）ID
using CpuPackageId = StrongId<std::uint32_t, CpuPackageIdTag>;
// CPU 物理核 ID
using CpuCoreId = StrongId<std::uint32_t, CpuCoreIdTag>;
// 逻辑 CPU（硬件线程）ID
using LogicalCpuId = StrongId<std::uint32_t, LogicalCpuIdTag>;
```

## 数据结构

### CpuPackage

CPU 物理封装（socket）。

```cpp
struct CpuPackage
{
    CpuPackageId id;                         // 封装 ID
    Vendor vendor;                           // 厂商
    DeviceName model_name;                   // 型号名称
    std::uint32_t physical_cores{};          // 物理核数
    std::uint32_t logical_threads{};         // 逻辑线程数
    std::optional<Frequency> base_frequency; // 基础频率（可能未知）
    std::optional<Frequency> max_frequency;  // 最大频率（可能未知）
};
```

### CpuCore

CPU 物理核。通过 `package_id` 指向所属封装。

```cpp
struct CpuCore
{
    CpuCoreId id;                        // 物理核 ID
    CpuPackageId package_id;             // 所属封装 ID
    std::uint32_t logical_threads{};     // 该核上的逻辑线程数
    std::optional<NumaNodeId> numa_node; // 所属 NUMA 节点（可能未知）
};
```

### LogicalCpu

逻辑 CPU（硬件线程）。通过 `core_id` 指向所属物理核，并通过
`package_id` 反范式化指向所属封装。

```cpp
struct LogicalCpu
{
    LogicalCpuId id;                     // 逻辑 CPU ID
    CpuCoreId core_id;                   // 所属物理核 ID
    CpuPackageId package_id;             // 所属封装 ID（反范式化）
    std::optional<NumaNodeId> numa_node; // 所属 NUMA 节点（可能未知）
    bool visible_to_current_process{};   // 当前进程是否可见
};
```

### NumaNode

单个 NUMA 节点的基本信息。

```cpp
struct NumaNode
{
    NumaNodeId id;                  // NUMA 节点 ID
    std::vector<LogicalCpuId> cpus; // 该节点包含的逻辑 CPU 列表
};
```

`NumaNode` 仅描述 CPU 侧的 NUMA 归属关系。各 NUMA 节点的内存信息见
`memory.md` 中的 `NumaMemory`。

### Cpu

CPU 子系统聚合，持有封装、物理核、逻辑 CPU、NUMA 节点与 ISA 扩展
列表，并提供层级关系查询与可见性筛选接口。

```cpp
struct Cpu
{
    Arch arch{};                               // CPU 架构
    std::vector<CpuPackage> packages;         // 物理封装列表
    std::vector<CpuCore> cores;               // 物理核列表
    std::vector<LogicalCpu> logical_cpus;     // 逻辑 CPU 列表
    std::vector<NumaNode> numa_nodes;         // NUMA 节点列表
    std::vector<IsaExtension> isa_extensions; // 支持的 ISA 扩展列表
    std::vector<CpuCache> caches;             // CPU 缓存实例列表（按层级/类型）
    std::string governor;                     // cpufreq 调频策略（如 performance）
    std::vector<ThermalZone> thermal_zones;   // 温度传感器列表

    // 按封装 ID 查找封装
    const CpuPackage* find_package(CpuPackageId id) const;
    // 按物理核 ID 查找物理核
    const CpuCore* find_core(CpuCoreId id) const;
    // 按逻辑 CPU ID 查找逻辑 CPU
    const LogicalCpu* find_logical_cpu(LogicalCpuId id) const;
    // 获取指定封装下的全部逻辑 CPU
    std::vector<const LogicalCpu*> logical_cpus_of_package(CpuPackageId id) const;
    // 获取指定物理核上的全部逻辑 CPU
    std::vector<const LogicalCpu*> logical_cpus_of_core(CpuCoreId id) const;
    // 获取指定封装下的全部物理核
    std::vector<const CpuCore*> cores_of_package(CpuPackageId id) const;
    // 获取当前进程可见的全部逻辑 CPU
    std::vector<const LogicalCpu*> visible_logical_cpus() const;
};
```

### CpuCache

单个缓存实例，按层级（L1/L2/L3）与类型（Data/Instruction/Unified）区分。

```cpp
struct CpuCache
{
    std::uint32_t level;     // 缓存层级（1 = L1, 2 = L2, ...）
    CacheType type;          // 缓存类型
    MemorySize size;         // 缓存大小（字节）
    std::uint32_t ways;      // 相联度
    std::uint32_t line_size; // 缓存行大小（字节）
    std::uint32_t cpu_number; // 采样来源的逻辑 CPU 编号
};
```

### ThermalZone

单个温度传感器。

```cpp
struct ThermalZone
{
    std::string name; // 传感器名称（如 thermal_zone0）
    std::string type; // 类型（如 x86_pkg_temp）
    Temperature temp; // 当前温度（毫摄氏度）
};
```

## 设计说明

- **`LogicalCpu::package_id` 反范式化**：逻辑 CPU 已持有 `core_id`，理论上
  可以通过 `core_id → CpuCore::package_id` 两步查找得到封装。但在高频访问
  场景下两步查找既不便于使用也增加出错面，因此在 `LogicalCpu` 上冗余存储
  `package_id`，使单条记录即可定位所属封装。
- **`numa_node` 直接从 sysfs 读取**：`CpuCore::numa_node` 与
  `LogicalCpu::numa_node` 直接来自 `/sys/devices/system/node` 下的映射
  （如 `cpulist`），不经过额外的拓扑解析层。
- **缓存与热区按采样 CPU 记录**：`CpuCache` 带 `cpu_number` 标注来源逻辑 CPU，
  `ThermalZone` 逐一列出全部热区。两者均来自 sysfs，读取不到时静默为空，
  不产生 warning。
