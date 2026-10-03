# 存储健康采集设计

## 范围与只读约束

新增独立 `Collect::StorageHealth`。默认完整采集与 SystemCard storage/health 请求该域，
基础与 compact 采集保持轻量。只执行固定的健康查询，不允许用户插入控制参数。
不更改系统配置、SMART 开关、缓存、电源管理、告警阈值，不执行自检、扫描或自动提权。
不安装工具，不更新 smartmontools 数据库，不自动枚举硬件 RAID 的物理端口。

NVMe 优先 `smartctl --json=v -H -A -d nvme /dev/nvmeN`，仅工具不存在时降级到
`nvme smart-log /dev/nvmeN -o json`。按 sysfs 控制器枚举并查询一次；报告是控制器级，
通过实际 sysfs 祖先关系关联非分区块设备，不把同一控制器计数复制成多块盘的计数。

直接 ATA 设备用 `smartctl --json=v -H -A -d ata -n standby,3,5`；SAS/SCSI 磁盘用
`smartctl --json=v -H -l error -d scsi -n standby,3`。固定类型避免自动识别探测。ATA 在低功耗
或无法检查功耗时退出；SCSI 工具会尝试低功耗检查，但驱动不支持时无法保证不唤醒。
只查询 sysfs 确认的整盘 SCSI disk（type=0），跳过 USB 桥、虚拟设备、分区、md/dm。
权限不足不重试；未支持路径保留未知，不能表示健康。

## 模型与语义

Storage 新增 health 报告数组。每个报告保留 target、关联设备列表、协议、来源、采集状态
及失败原因；SMART support/enabled/passed 均为可选字段，passed 只是设备报告的总体状态。
NVMe 保存 critical_warning 原始位掩码、备用空间及阈值、percentage_used、温度和累计计数。
percentage_used 是厂商寿命估计，可以超过 100，不推断即时失效。
128 位计数采用经验证的十进制字符串强类型，JSON 不经过浮点数；无法精确解析则缺失。
计数单位保持原协议（NVMe data units 为 1000 × 512 字节，不当作字节直接显示）。

ATA 展示工具给出的属性 ID/名称、归一化值、worst/threshold、原始显示文本、when_failed。
不把厂商原始属性按固定 ID 猜测为扇区数或寿命。SCSI 读取 read/write/verify 的明确
total_uncorrected_errors；正数属于历史记录，不表示此刻正在发生错误。

HardwareHealth 追加 drive_findings，保持原 md storage_alerts 兼容。类型化区分 SMART
总体失败、NVMe 当前 critical_warning、备用空间不足、寿命估计达到 100、ATA 当前属性
越界、历史属性越界、历史介质/不可纠正错误。历史类信息级，当前明确设备告警为警告或
严重；不从 unsafe shutdown 或通用 error-log 条目数推断硬件故障。

## 边界与实现

- Reader：独立 storage_health.cpp，实际 sysfs 枚举、设备关联证据和固定有界命令。
- Parser：独立 storage_health.cpp，纯 JSON 解析；SMART exit bits 0–2 是查询问题，
  3–7 是健康/历史证据，非零退出不能统一视为 I/O 失败。
- Serialization：独立模块负责报告对称读写与字段范围验证；旧快照缺省空数组。
- Resolver：小型纯函数生成 drive findings 和 storage_health 覆盖情况。
- Python：只展示 C++ 结论，新增独立存储健康展示模块，原 model.py 仅调用。

校验采用编译、实际只读采集、终端宽度、兼容构建和打包；遵从此前要求不新增或运行
本地单元测试。实机无权读取的内容如实标注，不能宣称验证了健康数据。

## 依据

- https://github.com/smartmontools/smartmontools/blob/master/smartmontools/smartctl.8.in
  （只读参数、低功耗检查、JSON 大整数、退出位掩码）
- https://github.com/linux-nvme/nvme-cli/blob/master/Documentation/nvme-smart-log.txt
  （控制器与命名空间范围）
- https://github.com/smartmontools/smartmontools/blob/master/smartmontools/nvmeprint.cpp
  （NVMe JSON 字段及原始单位）
