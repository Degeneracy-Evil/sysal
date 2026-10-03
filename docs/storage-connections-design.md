# 存储控制器与磁盘连接

## 模型

三个独立层次，不按内核对象数量推断物理控制器数量：

- `Storage.controllers`：PCI class 基类为 mass storage 的 PCI 功能，复用已有 PCI inventory，按 PCI 地址唯一。
- `Storage.nvme_controllers`：`/sys/class/nvme` 明确枚举的内核控制器，保留名称、transport、address、model、serial、firmware_rev、state、cntlid 和 subsysnqn。PCI 关联来自真实 sysfs 祖先；Fabrics address 原文保留，不强行当作 PCI 地址。
- `Storage.scsi_hosts`：`/sys/class/scsi_host` 枚举的内核 host，保留编号、proc_name、state、supported_mode、active_mode 和明确 PCI/ATA port 关联。一个 AHCI 控制器可能对应多个 host，这些不是多张物理控制器。

块设备追加独立 NVMe namespace / SCSI device 信息。namespace 编号只在其
子系统内有意义，不据此跨控制器匹配。SCSI H:C:T:L 的 LUN 使用 uint64。
分区通过现有 parent 关系指向整盘，不重复附加整盘协议信息。

## 读取与关联

仅读取固定 sysfs 属性及 canonical symlink 目标，新增独立 RawSource，保留失败原因。
不读取 config BAR，不执行 Identify/SCSI 命令，不扫描总线、不 rescan/reset/delete/connect/disconnect。

NVMe namespace 读取 nsid、nguid、eui；不读取 uuid：内核 uuid 属性可能回退到 NGUID，
读取还会触发一次内核警告，不能把该值宣称为真实 UUID。全零身份不冒充有效标识。
普通 namespace 的 controller 由 canonical 祖先明确关联；multipath head 仅沿
内核 `multipath` 目录的实际 symlink 逐路径关联，不按相同 nsid、NQN 或名称猜测。
旧内核没有这些 symlink 时保持缺失，不推断单一控制器。

SCSI 地址取整盘 `device` symlink 指向的内核设备对象名，严格解析四段 H:C:T:L。
ATA port 编号仅取已报告的 `ataN` sysfs 祖先，不从 host 编号推算，不宣称是背板槽位。
类型和状态保留驱动报告。硬件 RAID 暴露的逻辑卷仍是内核块设备，容量不声称代表后台物理盘。

## 展示与兼容

共享展示函数用于 storage 详情和 topology，概要不增加大表。PCI 控制器独立于磁盘
展示，包括当前没有块设备的控制器；多个 namespace、分区和路径不重复统计控制器。
新公开字段全部追加，旧 JSON 可读取；新整数严格验证类型和范围。Python 只展示公共模型。

## 本地源码依据

Linux 6.16.9：

- `drivers/nvme/host/sysfs.c`：namespace 身份、控制器属性、uuid 回退行为。
- `drivers/nvme/host/multipath.c`：namespace head 到 path 的 sysfs links。
- `drivers/scsi/scsi_sysfs.c`：host 字段和 H:C:T:L 设备名。
- `Documentation/ABI/testing/sysfs-class-scsi_host`：host 和物理控制器编号不能简单等同。

所有临时样本和构建放项目 tmp。以构建、实际只读采集、JSON 往返和必要静态检查
验证，不新增或运行本地单元测试。
