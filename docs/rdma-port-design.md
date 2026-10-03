# RDMA / InfiniBand 设备与端口

## 范围

本轮将内核 RDMA 设备与端口信息加入 Network，不采集软件安装信息，不使用
ibverbs、ibstat、ibnetdiscover 或主动 fabric 探测。只读取 sysfs 基础属性、设备
路径和关联接口目录；不读性能计数器、不打开 umad/issm、不修改端口、SM 或 GID。

## 模型与数据流

- `Network.rdma` 追加到现有 interfaces 后，包含 RDMA 类目录发现状态、缺失原因
  和设备清单。旧 JSON 缺失此字段时为 NotCollected / NotProvided，不能当作零设备。
- `RdmaDevice` 使用快照内 RdmaDeviceId 和内核设备名，保留 node_type、描述、GUID、
  固件、驱动、PCI、NUMA、同一 backing device 的接口与端口列表。
- `RdmaPort` 使用设备内 RdmaPortNumber；端口 0 有效（交换机管理端口）。保留逻辑
  状态、物理状态、链路层、内核速率报告及可解析的 bps、LID、SM LID/SL、LMC、
  capability mask 和明确的 GID 网络接口关联。
- Reader 写入追加的 RawSource::SysfsRdma，Parser 纯解析，Resolver 仅按准确 PCI
  地址补充设备身份与 NUMA。序列化放在独立 rdma 文件中，避免继续扩展大函数。
- canonical 路径使用 `<设备入口>/sysfs_path` 原始记录，这是一条派生路径证据，
  并非真实 sysfs 属性；`device/net/<接口>` 的空内容记录表示实际目录枚举结果。
- 设备与端口目录的成功发现各保留一条原始记录，基础属性读取失败仍保留身份。
  RDMA 类目录遍历成功且为空可表示当前未暴露设备；不存在或权限不足保留原因。
  发现状态只描述类目录枚举，不保证所有设备属性和端口都已读取。

## 关联与解释

- PCI 关联取 canonical 设备路径中最后一个完整 PCI 地址，不按设备名猜测厂商。
- `device/net` 只表示 backing device 的接口集合，不能据此将接口分配到具体端口。
- `ports/N/gid_attrs/ndevs/I` 的成功报告才表示明确端口接口关联；空 GID 槽读取
  可能失败，失败保留在来源观察中，不推断设备异常。关联按接口名去重，不读 GID 地址。
- InfiniBand 与 Ethernet 由 link_layer 明确区分；Ethernet 并不足以证明 RoCE 或 iWARP。
- rate 是内核报告的活动宽度与速度组合，保留包括 `4X EDR` 的完整字符串；可明确
  解析的十进制 Gb/sec 转为 Bandwidth（bps），不根据名字推断最大能力或应用吞吐量。
- 状态与 capability mask 反映驱动报告，不构建健康结论；端口 Down 不自动告警。
- LID/SM 等零值按有效报告保留，在 Ethernet 链路层下不解释其 InfiniBand 含义。
- 不假设 RDMA 设备与网络接口一一对应；纯 RDMA 设备没有 netdev 也必须保留。

## SystemCard

network 摘要仅追加少量 RDMA 发现/设备/已报告端口计数。详情展示设备关联、端口
状态与速率，其他身份和子网字段按设备/端口分组。topology 展示连接关系。
PCI 详情包括没有网络接口的 RDMA 设备。Python 只格式化公共字段，不复制解析逻辑。

## 验证

按当前开发约定不新增或运行本地单元测试。使用编译、定向静态检查、实机只读采集、
JSON 往返、窄屏预览与 CentOS 7 兼容构建。没有 RoCE 或多端口设备时明确保留验证限制。
所有临时产物写入项目 tmp/。

## 依据

- [Linux RDMA sysfs ABI](https://github.com/torvalds/linux/blob/master/Documentation/ABI/stable/sysfs-class-infiniband)
- [Linux RDMA sysfs 实现](https://github.com/torvalds/linux/blob/master/drivers/infiniband/core/sysfs.c)
