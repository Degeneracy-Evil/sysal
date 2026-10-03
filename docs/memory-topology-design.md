# 内存控制器与位置关系

## 数据边界

只读 Linux EDAC sysfs，不加载驱动、不修改标签、不清零计数器。固件的
`locator` / `bank_locator` 原样保留，不从厂商名称推断通道、CPU 或 NUMA。

EDAC inventory 独立于固件 DIMM inventory。每个 EDAC 条目由控制器编号、
种类（DIMM / rank）与控制器内编号标识。rank 容量属于一个 rank，不计入
DIMM 数，不把多个 rank 猜测合并成一根内存条。未能关联固件插槽的条目仍保留。

`dimm_location` 和 `max_location` 均为有序的“层名 编号”序列，层名包括
branch、channel、slot、csrow、memory。坐标只在完整格式合法且层名不重复时
结构化；否则保留原文。保留层级顺序，不假定 channel 是第一层或编号全局唯一。
`max_location` 是各层最大编号，不是安装数量，也不据此相乘推断物理插槽数。

## 插槽关联

没有固件库存时，仅用容量已知的 EDAC DIMM 条目构成回退库存，标记
`EdacInventory`；容量未知的条目继续留在独立 EDAC inventory 中。

固件和 EDAC 同时存在时，仅关联双方均唯一、非空标签相同、容量相同且
已安装的 DIMM，标记 `LabelAndCapacity`。EDAC 标签可由管理员更改，界面将此
关系明确标为标签与容量匹配，不宣称这是固件提供的硬件句柄关联。
rank 不参加此匹配，空槽也不参加。未匹配条目不按顺序或相似名称猜测。

## 展示与兼容

SystemCard 内存详情展示控制器边界、EDAC 位置及固件匹配依据；拓扑视图复用
位置表。概要保留简洁。新字段追加到公开结构体，旧 JSON 缺少这些字段时表示
未知；JSON 中存在的新编号严格验证整数类型及范围。

## 依据

核对本地 Linux 6.8.12 与 6.16.9 的以下文件：

- `Documentation/ABI/testing/sysfs-devices-edac`
- `drivers/edac/edac_mc.c` 的 `edac_dimm_info_location`、`edac_layer_name`
- `drivers/edac/edac_mc_sysfs.c` 的 `mci_max_location_show`、`edac_create_dimm_object`

对应上游文档：
[EDAC ABI](https://github.com/torvalds/linux/blob/master/Documentation/ABI/testing/sysfs-devices-edac)。
