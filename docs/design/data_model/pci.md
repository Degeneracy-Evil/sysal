# Pci

PCI 子系统描述系统中存在的 PCI 设备清单。

## 数据结构

### PciDevice

单个 PCI 设备。

```cpp
struct PciDevice
{
    PciAddress address;                  // PCI 地址
    Vendor vendor;                       // 厂商
    DeviceName device_name;              // 设备名称
    PciClass device_class;               // 设备类别
    std::optional<NumaNodeId> numa_node; // 所属 NUMA 节点（可能未知）
};
```

### Pci

PCI 子系统聚合，持有全部 PCI 设备并提供按地址查找接口。

```cpp
struct Pci
{
    std::vector<PciDevice> devices; // PCI 设备列表

    // 按 PCI 地址查找设备
    const PciDevice* find(PciAddress addr) const;
};
```

## 设计说明

- **Pci 是设备清单**：`Pci` 回答"系统中存在哪些 PCI 设备"，仅记录设备
  本身的属性（地址、厂商、名称、类别、NUMA 节点），不描述设备之间的
  父子拓扑关系。
- **`numa_node` 直接从 sysfs 读取**：来自
  `/sys/bus/pci/devices/<addr>/numa_node`，不经过额外的拓扑解析层。
  在不支持 NUMA 的系统或该字段缺失时为 `std::nullopt`。
