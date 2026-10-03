# PCIe 连接与能力

## 本轮范围

复用现有 current/max_link_speed、current/max_link_width、NUMA、插槽和固件标签，
补充直接上游 PCI 地址、当前绑定驱动、PF/VF 关联、本地 CPU 范围，以及报告的
SR-IOV 最大/已启用 VF 数。SystemCard 在 topology 展示连接，在 network/storage
详情中展示关联控制器及其上游路径；默认摘要不增加大表。

InfiniBand 端口和内存通道拓扑作为后续独立批次。

## 数据流

- Reader 仅读取 PCI sysfs 文本、符号链接和 canonical 目录路径；不打开 config、
  resource、VPD 或控制文件，不写 sysfs，不运行探测或配置命令。
- 已有 PCI 采集从通用 sysfs.cpp 提取到独立 pci.cpp，维持原有采集标志和来源枚举。
- canonical 路径以 `<入口>/sysfs_path` 存入 SysfsPci 原始记录，这是派生路径记录，
  并非真实 sysfs 属性。Parser 仅在直接父目录是完整 PCI 地址时设置 upstream_address。
- driver 与 physfn 原始记录保留 readlink 的目标；Parser 取驱动末段和 PF 地址。
- Parser 不读文件。Resolver 继续按明确 PCI 地址关联网卡和存储设备，不复制 PCI
  链路属性到每个关联设备，保持单一数据源。
- 所有新增字段追加到 PciDevice；JSON 对称可选，旧快照缺失时保持未知。

## 解释边界

- 速率保留内核报告及单位，位宽为 lane 数；零和内核 PCIE_LNK_WIDTH_UNKNOWN
  哨兵 255 不当作有效通道。旧快照的显示也遵守此规则。
- 最大能力属于该设备报告，不能当作整条路径的吞吐量，也不转换为应用带宽。
- NUMA=-1 保持未知；local_cpulist 是内核提供的亲和范围，NUMA 未知时可能回退
  到全部在线 CPU，不能由此推断插槽或 NUMA 节点。
- PF/VF 关系来自 physfn；VF 数为内核/驱动暴露值，未读取到不视为不支持 SR-IOV。
- 当前速率/位宽低于最大值不自动产生健康告警；省电、主板和上游能力都可能影响协商。
- 读链路属性可能由内核临时唤醒 runtime-suspended 设备，这是 sysfs 读取行为；
  不更改持久系统配置，不主动控制电源状态。
- 展示路径只沿明确上游地址，遇到缺失/循环停止，并明确标记，避免将不完整路径
  展示为完整主板拓扑。

## 验证

按当前开发约定不新增或执行本地单元测试；编译、定向静态检查、实际只读采集、
JSON 往返和终端不同宽度预览用于核实本轮变化。兼容发布仍由 CentOS 7 构建验证。

## 依据

- [Linux PCI sysfs ABI](https://github.com/torvalds/linux/blob/master/Documentation/ABI/testing/sysfs-bus-pci)
- [Linux PCI sysfs 实现](https://github.com/torvalds/linux/blob/master/drivers/pci/pci-sysfs.c)
- [Linux PCI sysfs 目录说明](https://www.kernel.org/doc/html/latest/PCI/sysfs-pci.html)
