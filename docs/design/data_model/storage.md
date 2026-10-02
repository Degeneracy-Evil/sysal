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
    std::optional<MountPoint> mount_point; // 挂载点
    std::optional<FilesystemType> fs_type; // 文件系统类型
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

## 设备身份与内核配置

每个 StorageDevice 额外保留 sysfs 可读的 model、vendor、serial、firmware_revision、wwid。
NVMe 的 firmware_rev 优先于 SCSI rev；transport 只来自驱动明确报告的 device/transport，
不根据 sdX 或控制器名称推断 SATA/SAS。controller_name 来自同一 PCI 地址的系统设备名称。

logical_block_size、physical_block_size、minimum_io_size、optimal_io_size 均为 MemorySize（字节）。
rotational、read_only、removable 是可选 bool；numa_node 是可选 NumaNodeId。
scheduler 是 queue/scheduler 中方括号标出的当前调度器，none 保留原义。
size 永远按 Linux 定义的 512 字节扇区换算，与 logical_block_size 无关；乘法先检查溢出。

RAID 暴露的块设备是控制器提供的逻辑设备，型号与 rotational 等信息不能用来断言底层每块硬盘的情况。
旧 mount_point/fs_type 继续表示选取的挂载位置，不代表完整分区树；本层不执行 SMART 或介质扫描。

来源：[Linux block sysfs ABI](https://github.com/torvalds/linux/blob/master/Documentation/ABI/stable/sysfs-block)。
