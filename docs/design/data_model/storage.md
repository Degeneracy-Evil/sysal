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
- v0.0.4 新增 `mount_point` 和 `fs_type` 字段，旧版通过 `df -Th` 命令采集；当前采集使用 mountinfo。
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

## 分层设备与挂载

`/sys/class/block` 提供整盘、分区与虚拟设备，`device_number` 使用 Linux major/minor。
`partition_number` 与 `parent` 来自分区属性和 sysfs 父目录；`slaves` 列出内核明确报告的下层设备。
`layer` 区分 disk、partition、device-mapper、md、virtual；证据不足时为 unknown。
device-mapper 保留 `mapper_name` / `mapper_uuid`，包括内核暴露的 LVM 标识；不推断完整 VG/LV 配置。
md 保留 `raid_level`、`raid_state`、`raid_disks`、`raid_degraded`，不执行阵列操作。

`Storage.mounts` 保存 `/proc/self/mountinfo` 中当前进程命名空间的全部记录，
包括重复挂载、bind mount、伪文件系统和网络文件系统。每条记录有 mount/parent ID、
设备号、文件系统根目录、挂载路径、来源、挂载选项和只读状态；内核八进制路径转义会解码。
`block_device` 仅按 major/minor 与可见块设备精确关联；路径或设备名相似不构成关联证据。
旧 `mount_point` / `fs_type` 只保留一个代表挂载，优先根目录；旧 df 回放继续兼容。

容量属于各层独立设备，不能将整盘、分区与 device-mapper/md 容量相加。
SystemCard 的容量摘要只加总 disk 层；这是系统暴露的容量，硬件 RAID 后面的物理盘容量可能不可见。
