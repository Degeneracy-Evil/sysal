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

### Memory

内存子系统聚合。

```cpp
struct Memory
{
    MemorySize total_memory;                    // 系统内存总量
    std::optional<MemorySize> available_memory; // 可用内存（可能未知）
    std::vector<NumaMemory> numa_memory;        // 各 NUMA 节点内存信息
};
```

## 设计说明

- `total_memory` 来自 `/proc/meminfo` 中的 `MemTotal`。
- `available_memory` 来自 `/proc/meminfo` 中的 `MemAvailable`，在较老
  内核上可能缺失，因此为 `std::optional`。
- `numa_memory` 来自 `/sys/devices/system/node/nodeN/meminfo`，仅在系统
  存在多个 NUMA 节点时非空。
