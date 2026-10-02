# 普通硬件信息范围

采集目标是系统、内核、驱动和固件已经提供的硬件清单；不运行性能测试，也不维护型号参数猜测表。
此轮覆盖 CPU、整机/主板/机箱/固件、内存、存储和网络，从 Sysal 公共模型贯通至 SystemCard 展示。

| 领域 | 采集和展示内容 |
| --- | --- |
| CPU | 线程/核心/插槽/NUMA、online/present、SMT、架构、family/model/stepping/ARM IDs、microcode、完整能力、缓存实例和共享范围、独立频率策略 |
| 整机 | 厂商、型号、系列、版本、SKU、产品 UUID、序列号；与 OS machine-id 分别保留 |
| 主板和机箱 | 厂商、型号/版本、序列号、资产标签、SMBIOS 机箱类型；系统提供的 PCI 插槽/固件标签及当前/最大链路 |
| 固件 | BIOS 厂商/版本/日期、BIOS revision、EC firmware revision、是否观察到 EFI sysfs |
| 内存 | OS 总量/可用量、NUMA 总量/空闲量、插槽人口、逐 DIMM 容量/类型/标称和配置速率、rank/位宽/外形/电压/部件号/标识、EDAC 模式和 device width |
| 存储 | 暴露块设备容量/类型/挂载、型号/厂商/序列号/固件/WWID、明确 transport、PCI 控制器/NUMA、逻辑和物理块大小、I/O 大小、调度器、rotational/read-only/removable |
| 网络 | 接口/IP/当前 MAC、PCI 名称和 NUMA、驱动、报告链路速率、duplex/carrier、MTU、接口索引、物理端口名 |

## 数据原则

- 未读取或未提供的字段保持缺失；optional 数值/布尔能区分未知与零/false。
- 清理常见固件占位字符串，保留有效标识原文。普通用户无权读取的字段不自动提权。
- 系统只提供的清单可能不完整，例如 EDAC 插槽数、PCI 插槽号和 RAID 底层硬盘。
- CPU 制程/工艺、IPC、IMC 细节等没有可靠本机接口时不填；频率报告不当作测速结果。
- GPU 和软件检测的进一步扩展留待另行安排。

默认界面保持摘要；`systemcard --section system|cpu|memory|storage|network` 展开相应详情。
字段和来源详见 `docs/design/data_model/`。开发记录以 Git 提交为准。
