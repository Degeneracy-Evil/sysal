# Memory

内存子系统描述系统的内存总量与各 NUMA 节点的内存分布。

## 数据结构

### NumaMemory

单个 NUMA 节点的内存信息。

```cpp
struct NumaMemory
{
    NumaNodeId node;                     // NUMA 节点 ID
    MemorySize total;                    // 节点内存总量
    std::optional<MemorySize> available; // 可用内存（可能未知）
};
```

### DimmInfo

单条 DIMM 内存条信息。同一台服务器的内存代际（type）和运行频率（configured_speed）是全局一致的，因此提升到 `Memory` 级别。每条 DIMM 只保留插槽级属性。

```cpp
struct DimmInfo
{
    std::string locator;                               // 物理插槽定位符（如 CPU0_C0D0）
    std::string bank_locator;                          // 内存库定位符（如 NODE 0）
    MemorySize size{0};                                // 容量（字节）
    std::optional<TransferRate> speed_mts;            // 标称速率（MT/s）
    std::optional<Vendor> manufacturer;               // 制造商
    std::optional<std::string> part_number;           // 部件号
    std::optional<std::uint32_t> rank;                // rank 数
    std::optional<std::uint32_t> total_width;         // 总位宽（含 ECC）
    std::optional<std::uint32_t> data_width;          // 数据位宽
    std::optional<std::string> form_factor;           // 外形规格（如 DIMM）
    bool present{};                                    // 插槽是否已安装内存条
};
```

### Memory

内存子系统聚合。

```cpp
struct Memory
{
    MemorySize total_memory;                      // 系统内存总量
    std::optional<MemorySize> available_memory;   // 可用内存（可能未知）
    std::string memory_type;                      // 内存类型（如 DDR4、DDR5）
    std::optional<TransferRate> configured_speed_mts; // 实际配置速率（MT/s）
    std::vector<NumaMemory> numa_memory;          // 各 NUMA 节点内存信息
    std::vector<DimmInfo> dimms;                  // 各 DIMM 内存条信息
    std::optional<std::uint32_t> dimm_count;      // DIMM 插槽总数
    std::optional<std::uint32_t> populated_dimms; // 已安装内存条的 DIMM 数
};
```

## 设计说明

- `total_memory` 来自 `/proc/meminfo` 中的 `MemTotal`。
- `available_memory` 来自 `/proc/meminfo` 中的 `MemAvailable`，在较老
  内核上可能缺失，因此为 `std::optional`。
- `numa_memory` 来自 `/sys/devices/system/node/nodeN/meminfo`，仅在系统
  存在多个 NUMA 节点时非空。
- `dimms`、`dimm_count`、`populated_dimms` 来自 v0.0.4 新增的 DIMM 采集。
- **双源策略**：DIMM 信息优先通过 `udevadm info -e` 采集（提供最完整的
  字段，包括 locator、type、speed、manufacturer、part_number 等），
  当 udevadm 不可用或失败时，回退到 EDAC sysfs
  （`/sys/devices/system/edac`）获取基本 DIMM 信息。这种策略确保在
  容器环境和完整物理机上都能获取尽可能多的内存拓扑数据。
