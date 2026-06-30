# Storage

存储子系统描述系统中的块设备信息。

## 数据结构

### StorageDevice

单个存储设备。

```cpp
struct StorageDevice
{
    StorageId id;                          // 存储设备 ID
    DeviceName name;                       // 设备名称
    std::optional<MemorySize> capacity;    // 容量（可能未知）
    std::optional<PciAddress> pci_address; // PCI 地址（可能无）
    StorageKind kind{};                    // 存储类型
};
```

### Storage

存储子系统聚合。

```cpp
struct Storage
{
    std::vector<StorageDevice> devices; // 存储设备列表
};
```

## 设计说明

- v0.0.1 仅提供基本设备清单：设备名、容量、类型与可选的 PCI 地址，
  不涉及分区、文件系统、挂载点、SMART 健康状态等更详细的信息，这些
  将在后续版本扩展。
