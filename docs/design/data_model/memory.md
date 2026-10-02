# Memory

内存信息区分操作系统可用容量、NUMA 空闲容量与物理 DIMM 配置。

## 字段与来源

- `Memory.total_memory` / `available_memory`：`/proc/meminfo` 的 MemTotal / MemAvailable，单位字节。
- `NumaMemory.total` / `free`：nodeN/meminfo 的 MemTotal / MemFree。MemFree 不等于 MemAvailable；现有可选 `available` 字段继续支持旧快照，新采集不以 free 填充它。
- `DimmInfo`：插槽/Bank、容量、是否安装、制造商、部件号、序列号、资产标签、外形、rank、数据/总位宽、类型和 type_detail。
- `speed_mts` 是标称速率；逐条 `configured_speed_mts` 是固件报告的配置速率，均使用 TransferRate（MT/s）。
- `configured_voltage_mv` 使用 Millivolts，保留 udev 报告的电压换算值，无法恢复来源已经舍弃的精度。
- `edac_mode` / `device_width` 保留 EDAC 报告的纠错模式及 DRAM device type，如 SECDED / x8，不以名称猜测 ECC 状态。

DIMM 优先使用 `udevadm info -e` 的 MEMORY_DEVICE_n_* 属性，没有有效 DIMM 时回退 EDAC sysfs。
EDAC 的插槽目录存在不代表安装内存，presence 根据有效 size 判定。
缺失或占位身份保持缺失；大小、速率、位宽与电压不会用零冒充未知值。

## 聚合

`memory_type` 在有效逐条类型不同的时候为 mixed。
`Memory.configured_speed_mts` 仅在所有已安装 DIMM 均报告相同配置速率时给出。
插槽数是当前数据源观察到的插槽数，不能视为物理插槽总数的保证。
udev 没有属性、旧内核没有 EDAC 或权限不足时，DIMM 字段允许缺失，不自动提权。

来源：[systemd DMI memory 属性导出](https://github.com/systemd/systemd/blob/main/src/udev/dmi_memory_id/dmi_memory_id.c)。
