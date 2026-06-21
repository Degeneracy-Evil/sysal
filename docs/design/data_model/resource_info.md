# ResourceInfo

Describes the physical and logical resources of the system.

```cpp
struct ResourceInfo
{
    CpuSubsystem cpu;
    MemorySubsystem memory;
    AcceleratorSubsystem accelerators;
    NetworkSubsystem network;
    StorageSubsystem storage;
    PciSubsystem pci;
    TopologyInfo topology;
};
```

## CPU

CPU topology uses parent IDs to express the package → core → logical CPU hierarchy.

```cpp
using CpuPackageId = StrongId<uint32_t>;
using CpuCoreId    = StrongId<uint32_t>;
using LogicalCpuId = StrongId<uint32_t>;

struct CpuPackage
{
    CpuPackageId id;
    CpuVendor vendor;
    DeviceName model_name;
    uint32_t physical_cores;
    uint32_t logical_threads;
    std::optional<Frequency> base_frequency;
    std::optional<Frequency> max_frequency;
};

struct CpuCore
{
    CpuCoreId id;
    CpuPackageId package_id;               // parent
    uint32_t logical_threads;
    std::optional<NumaNodeId> numa_node;
};

struct LogicalCpu
{
    LogicalCpuId id;
    CpuCoreId core_id;                     // parent
    CpuPackageId package_id;               // denormalized for convenience
    std::optional<NumaNodeId> numa_node;
    bool visible_to_current_process;
};

struct CpuSubsystem
{
    Architecture arch;
    std::vector<CpuPackage> packages;
    std::vector<CpuCore> cores;
    std::vector<LogicalCpu> logical_cpus;
    std::vector<NumaNode> numa_nodes;
    std::vector<IsaExtension> isa_extensions;

    // convenience queries
    const CpuPackage* find_package(CpuPackageId id) const;
    const CpuCore* find_core(CpuCoreId id) const;
    const LogicalCpu* find_logical_cpu(LogicalCpuId id) const;
    std::vector<const LogicalCpu*> logical_cpus_of_package(CpuPackageId id) const;
    std::vector<const LogicalCpu*> logical_cpus_of_core(CpuCoreId id) const;
    std::vector<const CpuCore*> cores_of_package(CpuPackageId id) const;
    std::vector<const LogicalCpu*> visible_logical_cpus() const;
};
```

`LogicalCpu::package_id` is denormalized — derivable via `core_id → CpuCore::package_id` —
but stored directly to avoid two-step lookups on hot paths.

## Memory

```cpp
struct MemorySubsystem
{
    MemorySize total_memory;
    std::optional<MemorySize> available_memory;
    std::vector<NumaMemoryInfo> numa_memory;
};

struct NumaMemoryInfo
{
    NumaNodeId node;
    MemorySize total;
    std::optional<MemorySize> available;
};
```

## Accelerators

```cpp
enum class AcceleratorKind
{
    Gpu,
    Npu,
    Fpga,
    Other,
};

struct AcceleratorDevice
{
    AcceleratorId id;
    AcceleratorKind kind;
    Vendor vendor;
    DeviceName name;

    std::optional<PciAddress> pci_address;
    std::optional<NumaNodeId> nearest_numa_node;
    std::optional<MemorySize> memory_size;
    std::optional<DriverId> driver;

    bool visible_to_current_process;
};

struct AcceleratorSubsystem
{
    std::vector<AcceleratorDevice> devices;

    std::vector<const AcceleratorDevice*> by_kind(AcceleratorKind kind) const;
    std::vector<const AcceleratorDevice*> gpus() const;
    std::vector<const AcceleratorDevice*> npus() const;
    std::vector<const AcceleratorDevice*> fpgas() const;
    std::vector<const AcceleratorDevice*> visible() const;
    const AcceleratorDevice* find(AcceleratorId id) const;
};
```

`devices` is the source of truth; convenience methods are non-owning filters.

## Network

```cpp
struct NetworkInterface
{
    InterfaceName name;
    MacAddress mac;
    InterfaceState state;

    std::optional<Bandwidth> speed;
    std::vector<IpAddress> addresses;

    std::optional<PciAddress> pci_address;
    std::optional<RdmaDeviceId> rdma_device;

    bool visible_to_current_process;
};

struct NetworkSubsystem
{
    std::vector<NetworkInterface> interfaces;

    std::vector<const NetworkInterface*> visible() const;
    const NetworkInterface* find(const InterfaceName& name) const;
};
```

## PCI

```cpp
struct PciDevice
{
    PciAddress address;
    Vendor vendor;
    DeviceName device_name;
    PciClass device_class;
    std::optional<NumaNodeId> numa_node;   // denormalized from DeviceLocality
};

struct PciSubsystem
{
    std::vector<PciDevice> devices;

    const PciDevice* find(PciAddress addr) const;
};
```

`PciSubsystem` is the device inventory ("what exists").
`TopologyInfo` is the relationship graph ("how they connect").
`PciDevice::numa_node` is denormalized from `DeviceLocality` for convenience.

## Storage

```cpp
struct StorageDevice
{
    StorageId id;
    DeviceName name;
    std::optional<MemorySize> capacity;
    std::optional<PciAddress> pci_address;
    StorageKind kind;                     // Nvme, Sata, Sas, ...
};

struct StorageSubsystem
{
    std::vector<StorageDevice> devices;
};
```

Storage is minimal in v0.0.1.
