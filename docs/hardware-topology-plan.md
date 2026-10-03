# 硬件拓扑与数据来源设计

本轮补齐五项：存储拓扑、网络能力、内存控制器、跨设备关联、来源与缺失原因。

## 公共模型

1. **存储**：统一列出整盘、分区和虚拟块设备；追加 major/minor、分区父设备、slaves、device-mapper name/UUID、md 属性；所有挂载记录独立建模，按设备号关联，不通过名称前缀猜测。保留旧代表性挂载字段以兼容旧调用方。
2. **网络**：现有内核接口信息补充固件/驱动版本、永久地址、支持/广告链路模式和自动协商；ethtool 为可选只读来源。sysfs/proc 提供 master/lower、bond/bridge/VLAN 信息。
3. **内存**：EDAC 控制器身份、容量、纠错计数及明确的设备/NUMA 关联；逐 DIMM 只按唯一标签与容量匹配补充控制器关系。标注 DIMM 来源、固件报告槽数及清单完整程度，不按标签猜测 CPU。
4. **拓扑展示**：由现有强类型关系生成 NUMA/PCI/块设备/网络关联视图；`--section topology` 明确请求相关采集域，默认界面不重复展示完整关系。
5. **来源**：原始记录补充读取失败分类；meta 保留无 payload 的采集观察。`--sources` 展示来源路径和采集结果，明确权限不足、接口缺失、不支持、读取失败，旧快照的原因缺失时保持未知。

## 原则

- C++ Reader 执行 I/O，Parser 处理原始证据，Resolver 关联明确标识；Python 只展示公共 JSON。
- 兼容旧 JSON；新字段默认缺失。UUID、挂载路径及接口名不经未经转义的 shell 拼接。
- 正确处理 mountinfo 的转义、多重挂载、bind mount 和命名空间视角；容量求和避开分区/虚拟层重复计数。
- 不运行网络、磁盘、内存基准测试；不触发介质扫描、SMART 自检或自动提权。
- 临时文件放各项目 tmp；本地以编译、实际采集和必要的定向静态检查验证，不新增或运行本地单元测试。

## 来源

- [Linux proc/mountinfo](https://docs.kernel.org/filesystems/proc.html)
- [Linux ethtool 查询](https://docs.kernel.org/networking/ethtool-netlink.html)
- [Linux EDAC ABI](https://github.com/torvalds/linux/blob/master/Documentation/ABI/testing/sysfs-devices-edac)
- [Linux 网络 sysfs ABI](https://github.com/torvalds/linux/blob/master/Documentation/ABI/testing/sysfs-class-net)

所有关联仅代表当前进程能观察到的系统视图；未提供的关系保留未知。
