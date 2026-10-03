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

- **Pci 是设备清单和明确的连接关系**：`Pci` 保留设备自身属性及直接上游地址。
  不根据总线编号猜测拓扑，不把根目录当作可枚举的 PCI 设备。
- **`numa_node` 直接从 sysfs 读取**：来自
  `/sys/bus/pci/devices/<addr>/numa_node`，不经过额外的拓扑解析层。
  在不支持 NUMA 的系统或该字段缺失时为 `std::nullopt`。

## 插槽、固件标签与链路

PciDevice 可保留 physical_slot、firmware_label（sysfs label），以及 current/max_link_speed、current/max_link_width。
速率保持内核自带单位的字符串，位宽为可选 lane 数；缺失不以零代替。
当前协商链路与设备报告的最大链路分别保留，不能视为整条上游路径的可用吞吐量。
没有插槽号或固件标签时保持缺失，不按总线号或设备名猜测插槽位置。

来源：[Linux PCI sysfs ABI](https://github.com/torvalds/linux/blob/master/Documentation/ABI/testing/sysfs-bus-pci)。

## 上游连接、驱动与虚拟功能

- `upstream_address`：canonical sysfs 路径的直接父目录为完整 PCI 地址时设置。
- `physical_function`：`physfn` 目标中的 PF 地址，表示明确的 VF/PF 关系。
- `driver_name`：带语义类型的当前绑定驱动名称，来自 `driver` 链接末段。
- `local_cpus`：`local_cpulist` 解析出的 LogicalCpuId；不能用于反推未知 NUMA。
- `maximum_virtual_functions` / `enabled_virtual_functions`：`sriov_totalvfs` /
  `sriov_numvfs` 报告值，零为有效值，缺失不表示不支持。

新增字段追加在原有字段后，保持旧聚合初始化顺序；旧 JSON 缺失字段时保持默认值。
完整边界和来源见 [PCIe 设计](../../pcie-topology-design.md)。
