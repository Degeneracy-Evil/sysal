# Platform

描述系统的基本标识：运行在什么主机上、什么操作系统、什么内核、什么架构，
以及固件和虚拟化情况。`Platform` 是 `SystemInfo` 的第一层，回答
"这是一台什么样的机器"。

## Platform 结构体

```cpp
namespace sysal
{

struct Platform
{
    Host                   host;            // 主机标识
    Os                     os;              // 操作系统
    Kernel                 kernel;          // 内核
    Architecture           architecture;    // 硬件架构
    std::optional<Firmware>       firmware;        // 固件（可能采集不到）
    std::optional<Virtualization> virtualization;  // 虚拟化（可能采集不到）
};

}  // namespace sysal
```

`firmware` 与 `virtualization` 为 `std::optional`：在容器或某些环境下
可能无法获取，缺失时不影响其余字段。

## 子结构体

| 结构体 | 说明 | 示例字段 |
|---|---|---|
| `Host` | 主机标识 | `hostname`、`machine_id`、`product_name`、`vendor`、`serial` |
| `Os` | 操作系统 | `name`、`version`、`distribution`、`distribution_version`、`codename` |
| `Kernel` | 内核 | `release`、`version`、`compiled_at`、`architecture` |
| `Architecture` | 硬件架构 | `name`（如 `x86_64`/`aarch64`）、`bits`（64/32）、`byte_order` |
| `Firmware` | 固件 | `bios_vendor`（Vendor）、`bios_version`、`bios_date`、`uefi`（bool） |
| `Virtualization` | 虚拟化 | `kind`（None/KVM/Xen/VMware/...）、`hypervisor` |

## 设计说明

* `Platform` 只描述"机器本身"，不涉及 CPU 核数、内存大小等资源数量——
  那些属于 `Cpu` / `Memory` 等资源子域。
* `Virtualization::kind` 为枚举，`None` 表示物理机，非容器非虚拟化。
* 字段命名遵循 `snake_case`，类型名遵循 `PascalCase`。
