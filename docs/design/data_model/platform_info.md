# PlatformInfo

描述系统的基本标识信息。

```cpp
struct PlatformInfo
{
    HostInfo host;
    OsInfo os;
    KernelInfo kernel;
    ArchitectureInfo architecture;
    std::optional<FirmwareInfo> firmware;
    std::optional<VirtualizationInfo> virtualization;
};
```

子结构体（`HostInfo`、`OsInfo`、`KernelInfo`、`ArchitectureInfo`、`FirmwareInfo`、
`VirtualizationInfo`）是在实现阶段定义的简单类型化聚合体。

| 子结构体 | 示例字段 |
|---|---|
| `HostInfo` | 主机名 |
| `OsInfo` | 操作系统名称、版本 |
| `KernelInfo` | 内核版本 |
| `ArchitectureInfo` | CPU 架构、机器架构 |
| `FirmwareInfo` | BIOS 版本、厂商 |
| `VirtualizationInfo` | hypervisor 类型、容器基础信息 |
