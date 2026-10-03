# Sensors 与 HardwareHealth

Sensors 是独立采集域（Collect::Sensors），读取 hwmon 与 thermal zone 的公开只读属性。
TemperatureSensor、FanSensor、PowerSensor 分别使用有符号毫摄氏度、RPM、微瓦。
保留 input、average、minimum、maximum、critical、enable、fault 和实际提供的 alarm 标志。
温度 type=4 只标识热敏电阻，不能证明单位。it87、ntc-thermistor 驱动明确转换为摄氏度；
其他无法明确单位的热敏电阻通道保留身份与状态，unit_supported=false，不擅自转换。

SensorIdentity 包含当前快照内 ID、芯片/通道、原始标签、来源路径、设备路径和明确 PCI 地址。
hwmon 序号不是跨启动硬件身份；无明确关系的 thermal/hwmon 不按相同读数去重。
有明确 PCI 显示设备类的 hwmon 不纳入本轮采集。
旧 Cpu.thermal_zones 继续兼容；无法表达的负温度不会被转换为零或无符号大整数。

HardwareHealth 在 Resolver 后生成，只使用本次已有采集结果，不执行额外 I/O。
SensorAlert 区分驱动告警、读数故障及明确阈值越界；每通道保留最直接的一项证据，
驱动标志优先于单次读数比较。禁用或故障通道不比较读数；上下限矛盾时不推断越界。
风扇零转速自身不形成告警。读数达到报告临界值不等同于通用硬件温度判断。

StorageAlert 保留 md 报告的降级成员数。MemoryErrorEvent 保存 EDAC 自初始化/重置以来的
已纠正与不可纠正累计错误，不能解释为当前错误速率或正在发生的故障。
HealthCoverage 分域表达未请求、无可用证据、部分证据、有可用证据；不保证所有硬件均可见。
无事件不生成“整机正常”的结论，功耗读数也不能证明电源在位或冗余供电正常。

新增 JSON 字段缺少时默认构造；枚举与 Collect 编号仅追加。新传感器整数反序列化校验
符号及取值范围，旧 Temperature 单位语义保持兼容。

设计与来源见 [hardware-health-design](../../hardware-health-design.md)。

单位例外依据：[it87](https://docs.kernel.org/hwmon/it87.html)、[ntc_thermistor 实现](https://github.com/torvalds/linux/blob/master/drivers/hwmon/ntc_thermistor.c)。
