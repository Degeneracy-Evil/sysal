# Network

网络子系统描述系统的网络接口信息。

## 数据结构

### NetworkInterface

单个网络接口。

```cpp
struct NetworkInterface
{
    InterfaceName name;                      // 接口名称
    MacAddress mac;                          // MAC 地址
    InterfaceState state{};                  // 链路状态
    std::optional<Bandwidth> speed;          // 链路速率（可能未知）
    std::vector<IpAddress> addresses;        // 绑定的 IP 地址列表
    std::optional<PciAddress> pci_address;   // PCI 地址（可能无）
    bool visible_to_current_process{};       // 当前进程是否可见
};
```

### Network

网络子系统聚合，持有全部网络接口并提供可见性筛选与按名查找接口。

```cpp
struct Network
{
    std::vector<NetworkInterface> interfaces; // 网络接口列表

    // 获取当前进程可见的接口
    std::vector<const NetworkInterface*> visible() const;
    // 按接口名查找
    const NetworkInterface* find(const InterfaceName& name) const;
};
```

## 硬件与配置详情

接口新增 driver、physical_port_name、interface_index、mtu、carrier、duplex、numa_node。
均来自 sysfs；MTU 使用 MemorySize（字节），carrier 使用 optional bool。
接口的 speed 是内核报告的最新/当前链路速率，不代表网卡最大能力，不通过流量测速推断。
未知、负值或读取失败保持缺失；duplex 只接收 full/half，不将 unknown 当作有效配置。
MAC 是当前接口地址，不能保证永久硬件地址，InfiniBand 等接口也不限制为六字节。

通过明确 PCI 地址关联 vendor 和 device_name；vendor 通常为数值 PCI 厂商 ID，
系统 PCI 数据库不存在时名称可能仍为设备 ID。Network 与 Storage 的采集会读取辅助 PCI 数据，
不改变 meta 中用户请求的 flags。虚拟接口没有 PCI 地址时不猜测型号或 NUMA 关联。

来源：[Linux network sysfs ABI](https://github.com/torvalds/linux/blob/master/Documentation/ABI/testing/sysfs-class-net)。

## 驱动能力与接口关系

物理接口通过可选的只读 ethtool 查询补充 `driver_version`、`firmware_version`、
`permanent_mac`、`autonegotiation`，以及 supported/advertised/peer link modes。
永久地址未知或全零时保持缺失，不能由当前 MAC 替代。
链路模式来自驱动能力报告，与当前 `speed` 分开；不会从当前速率推断最大能力。
查询可能在提供部分数据的同时报告权限错误：保留可用输出，来源状态标为 Partial。
工具不存在、内核不支持、权限不足或超时均允许缺失，不自动提权。

`master` 与 `lower_interfaces` 来自 sysfs 链接；`interface_kind` 保留 physical、virtual、
bridge、bond、vlan 分类；`bond_mode` 来自 bonding/mode。
`vlan_id` / `vlan_parent` 来自 `/proc/net/vlan/config` 的明确记录。
这些关系描述当前可见接口，不通过接口名猜测 VLAN、bond 或 bridge。
