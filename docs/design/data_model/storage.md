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
    std::optional<std::string> mount_point; // 挂载点
    std::optional<std::string> fs_type;     // 文件系统类型
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

- v0.0.3 仅提供基本设备清单：设备名、容量、类型与可选的 PCI 地址。
- v0.0.4 新增 `mount_point` 和 `fs_type` 字段，通过 `df -Th` 命令采集。
  挂载点和文件系统类型为 `std::optional`，因为部分设备（如未挂载的
  NVMe 盘）可能没有挂载信息。
