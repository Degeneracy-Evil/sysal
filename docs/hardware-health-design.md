# 传感器与硬件异常摘要设计

## 本轮范围

先实现本地传感器采集与基于已有证据的异常摘要。温度、风扇、功耗优先使用 Linux
hwmon；thermal zone 作为独立来源保留。电源告警仅展示驱动明确暴露的状态。
不将功耗数值当作电源健康结论；系统未提供电源状态时明确未知。

存储 SMART/NVMe 健康查询放在后续独立阶段。本轮异常摘要使用已有 md、EDAC 和新增
传感器数据，完成后收敛默认展示。保持 Python 3.6.8、C++20 和 glibc 2.17 兼容要求。

## 现状与需要修正的边界

- CPU 模型已有 thermal_zones，但 thermal zone 不一定属于 CPU。
- 实机 hwmon 暴露了 NVMe 和网卡温度，目前没有观察到 CPU、风扇、电源通道。
- 温度现有单位使用无符号整数，新传感器模型必须支持负温度。
- 已有 md 降级数、EDAC CE/UE 累计计数可以复用，不重复执行采集。
- 采集失败和硬件告警是不同信息，分别保留在 meta.observations 与硬件健康模型中。

## 数据模型

新增独立 `Sensors` 域，追加 `Collect::Sensors`，不移动已有 Collect 位或 RawSource 编号。
`SystemInfo` 追加 sensors 和 hardware_health，旧 JSON 缺少时默认构造。

传感器按类型保存为 temperatures、fans、powers 三个数组，避免通用数值字段搭配字符串单位。
每种类型复用一个简单身份结构，不引入传感器插件或通用规则框架：

| 结构 | 内容 |
| --- | --- |
| SensorIdentity | 当前快照内的 ID、芯片名、通道名、原标签、来源、读取路径、可选明确设备/PCI 关联 |
| TemperatureSensor | 有符号毫摄氏度读数、驱动报告的上下限/临界值、可选 enable/fault/alarm 状态 |
| FanSensor | RPM 读数、驱动报告的上下限、可选 enable/fault/alarm 状态 |
| PowerSensor | 微瓦读数、单独的平均功耗与驱动限值、可选 enable/fault/alarm 状态 |

温度新增有符号强类型，不全局修改现有 ScalarUnit 的整数语义。RPM、微瓦也使用专用强类型；
JSON 保留整数原始单位，Python 只做显示换算。所有可选读数允许缺失，零值不自动变成未知。

ID 由来源路径和通道标识形成，仅保证当前快照内唯一；hwmonN 不作为跨启动的硬件身份。
缺少标签时显示芯片名与通道名，不把 temp2、fan2 或标签里的 CPU 字样当作硬件归属证据。
thermal 与 hwmon 没有明确同源关系时分别保留，不按数值相同去重。

健康结果采用三个明确的集合：sensor_alerts、storage_alerts、memory_events。
各项使用对应传感器 ID、块设备名或控制器编号，保留类型化原因、级别和证据来源。
Memory events 明确标记为自初始化/重置以来的累计事件，不伪装为当前故障。
另有域覆盖说明，记录未请求、无可用证据、部分可用或有可用证据；不输出“整机健康”布尔值。

## 代码边界

| 层 | 职责与拟新增模块 |
| --- | --- |
| Reader | reader/linux/sensors.cpp：枚举并只读实际存在的 hwmon/thermal 属性，记录 I/O 结果 |
| Parser | parser/sensors.cpp：校验属性与整数单位，生成上述传感器模型，不执行 I/O |
| Resolver | 复用明确设备标识关联，不按名字或数组位置猜测硬件归属 |
| 健康归纳 | resolver/hardware_health.cpp：对完成关联的模型运行少量纯函数，生成明确事件及覆盖说明 |
| Serialization | 延续现有序列化实现，为新增结构提供对称读写与旧快照默认值 |
| Binding | 只增加 section/Collect 映射，继续调用公共 collect/to_json |
| Python | sensors.py、health.py 构建展示；model.py 仅注册新卡片，不添加采集和告警规则 |

健康结果在 Resolver 完成后生成，只消费本次已请求并采集的数据，不隐式追加 I/O。
不把硬件事件塞进现有 warnings 字符串；warnings 继续表示采集/解析问题。

## 采集与解释规则

1. 枚举实际文件，再读取支持的 input、label、limit、enable、fault、alarm 属性；
   不对每个芯片穷举不存在的文件，不读取或写入 PWM、复位及控制属性。
2. 每个读数保留实际来源；一个通道失败不丢弃其他通道。没有 hwmon/thermal 数据时保持未知。
3. 温度单位按 ABI 解释；非标准温度通道不能直接当作摄氏度，无法明确解释时标记不支持。
   功耗 input 与 average 分开，风扇值直接使用 RPM，不额外应用 divisor。
4. 有明确 PCI 显示设备类的 hwmon 本轮跳过；归属未知的芯片保持未知，不通过厂商名推断类别。
5. 电源可能由 PMBus 等驱动暴露为多个 hwmon 通道；只显示原始标签、读数和明确告警，
   不从设备数量推断冗余电源数量、在位状态或供电正常。
6. CPU 旧 thermal_zones 字段继续兼容；展示不得把全部传感器的最高温当作 CPU 温度。

## 首批健康归纳规则

| 证据 | 结果 | 语义限制 |
| --- | --- | --- |
| 驱动 alarm 或 fault 明确置位 | 传感器告警/读数故障 | 不将驱动告警与阈值比较混为一谈，状态可能锁存 |
| 可用读数达到驱动明确临界上限 | 当前读数达到报告临界值 | 显示读数、限值与来源，不设置统一温度阈值 |
| 可用读数越过驱动明确 min/max | 超出报告范围 | 禁用或 fault 通道不参与读数比较 |
| md raid_degraded > 0 | 阵列降级 | 表示报告的冗余成员缺失，不推断阵列已不可用 |
| EDAC uncorrected_errors > 0 | 曾记录不可纠正错误 | 明确累计时间语义，不声称当前正在发生 |
| EDAC corrected_errors > 0 | 曾记录已纠正事件 | 信息级别，不由单次累计值推断频率或故障趋势 |

风扇零转速本身不触发故障，缺少阈值或告警时保留零值。未知阈值不参与判断。
零个事件只表示本次可见证据中没有发现上述事件；缺少来源不等于正常。
同一通道同类证据合并展示，明确驱动告警优先，避免同一异常重复堆行。

## CLI 与默认展示

- `--section sensors`：Platform + Sensors，显示温度、风扇、功耗、限值和状态。
- `--section health`：Platform + Sensors + Memory + Storage + Pci，汇总已有 md/EDAC/传感器证据。
- `--sources`：复用来源观察，新增 sensors 域；health 选择其实际依赖域。
- 默认采集补充 Sensors；默认卡片只加紧凑告警摘要，完整传感器清单按 section 展开。
- 历史内存计数与当前告警分组显示；没有告警时不输出绿色“全部正常”。
- 保持现有宽度适配；不在 Python 中重新计算阈值或解释驱动状态。

## 实现顺序与完成条件

1. 公共强类型与 JSON 字段：单位、枚举、Collect、来源及旧快照兼容。
2. 独立传感器 Reader/Parser，接通实机数据；同步澄清 CPU 温度展示。
3. 独立健康归纳模块，复用 md/EDAC 与传感器模型，补齐覆盖说明。
4. 薄绑定与两个展示模块，最后整理默认摘要和来源输出。
5. 编译、实际采集、小窗口展示、CentOS 7 兼容构建与打包验证；不新增或运行本地单元测试。

这台机器没有暴露的风扇、电源及 EDAC 数据不能宣称已经实机验证；需要后续真实机器样本。
每一步完成后保持可编译，避免一次性修改多个层后才发现接口不成立。

## 依据

- [Linux hwmon sysfs ABI](https://docs.kernel.org/hwmon/sysfs-interface.html)：可选属性、单位、标签与驱动告警语义。
- [Linux EDAC 文档](https://docs.kernel.org/driver-api/edac.html)：纠错与不可纠正错误报告。
- 既有强类型规则与 Reader → RawStore → Parser → Resolver 结构。
