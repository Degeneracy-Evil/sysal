# Platform

平台信息回答“这是一台什么机器”：主机、操作系统、内核、架构、整机、主板、机箱、固件和虚拟化。
所有固件身份均来自 Linux 导出的 `/sys/class/dmi/id` 文本，不根据型号查询或推断。

## 数据结构

| 结构体 | 字段 |
| --- | --- |
| `Host` | hostname、machine_id、vendor、product_name、serial、product_family、product_version、product_sku、product_uuid |
| `Baseboard` | vendor、name、version、serial、asset_tag |
| `Chassis` | vendor、可选 SMBIOS type 编号、version、serial、asset_tag |
| `Firmware` | bios_vendor、bios_version、bios_date、bios_release、ec_firmware_release、uefi |
| `Os` | name、version、distribution、distribution_version、codename |
| `Kernel` | release、version、compiled_at、architecture |
| `Architecture` | name、bits、byte_order |
| `Virtualization` | kind、hypervisor |

`Platform.baseboard`、`chassis`、`firmware`、`virtualization` 均为可选值。
只有读到有效字段才创建主板、机箱和固件对象。新字段缺失时，旧 JSON 仍可载入。
空字符串表示未获取有效标识；数值和状态字段使用 optional 区分未知与零/false。

## 语义与边界

- `product_uuid` 是固件产品 UUID，`machine_id` 是操作系统安装标识，两者不能替代。
- `bios_version` 是厂商版本字符串；`bios_release` 是 SMBIOS BIOS revision，两者分别保留。
- `chassis.type` 保留系统提供的 SMBIOS 编号，不从机器型号推断机箱规格。
- `uefi=true` 表示观察到了 EFI sysfs；false 也可能是容器遮蔽，不证明 Legacy 启动。
- 常见占位字符串（如 Not Specified、Default String、NO DIMM）不作为有效身份；有效型号、版本及大小写保持原样。
- 内核通常限制普通用户读取整机 UUID 和整机/主板/机箱序列号；保持缺失，不自动提权。
- DMI 不存在的架构或容器仍可获取 OS、内核和主机名，其他硬件身份允许缺失。
- 虚拟化检测保留现有 sysfs、DMI 和 CPU 标志路径；容器属于 ExecutionContext。

字段来源参见 [Linux DMI sysfs 实现](https://github.com/torvalds/linux/blob/master/drivers/firmware/dmi-id.c)。
