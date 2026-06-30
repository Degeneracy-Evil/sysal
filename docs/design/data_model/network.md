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
