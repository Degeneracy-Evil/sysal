# PlatformInfo

Describes the basic identity of the system.

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

Sub-structs (`HostInfo`, `OsInfo`, `KernelInfo`, `ArchitectureInfo`, `FirmwareInfo`,
`VirtualizationInfo`) are straightforward typed aggregates defined during implementation.

| Sub-struct | Example fields |
|---|---|
| `HostInfo` | hostname |
| `OsInfo` | OS name, version |
| `KernelInfo` | kernel version |
| `ArchitectureInfo` | CPU architecture, machine architecture |
| `FirmwareInfo` | BIOS version, vendor |
| `VirtualizationInfo` | hypervisor type, container base |
