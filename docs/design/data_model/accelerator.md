# Accelerators

加速器子系统描述 GPU、NPU、FPGA 等异构加速设备。

## 枚举

### AcceleratorKind

加速器设备类型。

```cpp
enum class AcceleratorKind
{
    Gpu,   // GPU 加速器
    Npu,   // 神经网络处理器（NPU）
    Fpga,  // 现场可编程门阵列（FPGA）
    Other, // 其他或未知类型
};
```

## 数据结构

### AcceleratorDevice

单个加速器设备。

```cpp
struct AcceleratorDevice
{
    AcceleratorId id;                            // 加速器 ID
    AcceleratorKind kind{};                      // 加速器类型
    Vendor vendor;                               // 厂商
    DeviceName name;                             // 设备名称
    std::optional<PciAddress> pci_address;       // PCI 地址（可能无）
    std::optional<NumaNodeId> nearest_numa_node; // 最近 NUMA 节点（可能未知）
    std::optional<MemorySize> memory_size;       // 设备显存/内存（可能未知）
    std::optional<DriverId> driver;              // 关联驱动 ID（可能无）
    bool visible_to_current_process{};           // 当前进程是否可见
};
```

### Accelerators

加速器子系统聚合，持有全部加速器设备并提供按类型筛选、可见性筛选与
查找接口。

```cpp
struct Accelerators
{
    std::vector<AcceleratorDevice> devices; // 加速器设备列表

    // 按类型筛选加速器
    std::vector<const AcceleratorDevice*> by_kind(AcceleratorKind kind) const;
    // 获取全部 GPU
    std::vector<const AcceleratorDevice*> gpus() const;
    // 获取全部 NPU
    std::vector<const AcceleratorDevice*> npus() const;
    // 获取全部 FPGA
    std::vector<const AcceleratorDevice*> fpgas() const;
    // 获取当前进程可见的加速器
    std::vector<const AcceleratorDevice*> visible() const;
    // 按加速器 ID 查找设备
    const AcceleratorDevice* find(AcceleratorId id) const;
};
```

## 设计说明

- **`devices` 是数据真源**：`Accelerators` 仅持有 `devices` 一个容器，
  所有便利方法（`by_kind` / `gpus` / `npus` / `fpgas` / `visible` /
  `find`）都是非持有型过滤视图，返回指向 `devices` 内元素的指针，不
  复制数据，也不维护额外的索引容器。
- **`nearest_numa_node` 直接从 sysfs 读取**：来自 PCI 设备的
  `numa_node` 文件（`/sys/bus/pci/devices/<addr>/numa_node`），不经
  额外的拓扑解析层。若该设备无 PCI 地址或 sysfs 未暴露 NUMA 信息，则
  为 `std::nullopt`。
