# ResourceInfo

描述系统的物理与逻辑资源。

```cpp
struct ResourceInfo
{
    CpuSubsystem cpu;
    MemorySubsystem memory;
    AcceleratorSubsystem accelerators;
    NetworkSubsystem network;
    StorageSubsystem storage;
    PciSubsystem pci;
};
```

## CPU

CPU 拓扑通过父 ID 表达 package → core → 逻辑 CPU 的层次结构。

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
    CpuPackageId package_id;               // 父节点
    uint32_t logical_threads;
    std::optional<NumaNodeId> numa_node;    // 直接从 sysfs 读取
};

struct LogicalCpu
{
    LogicalCpuId id;
    CpuCoreId core_id;                     // 父节点
    CpuPackageId package_id;               // 为便利而反范式化
    std::optional<NumaNodeId> numa_node;   // 直接从 sysfs 读取
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

    // 便利查询方法
    const CpuPackage* find_package(CpuPackageId id) const;
    const CpuCore* find_core(CpuCoreId id) const;
    const LogicalCpu* find_logical_cpu(LogicalCpuId id) const;
    std::vector<const LogicalCpu*> logical_cpus_of_package(CpuPackageId id) const;
    std::vector<const LogicalCpu*> logical_cpus_of_core(CpuCoreId id) const;
    std::vector<const CpuCore*> cores_of_package(CpuPackageId id) const;
    std::vector<const LogicalCpu*> visible_logical_cpus() const;
};
```

`LogicalCpu::package_id` 是反范式化字段——可通过 `core_id → CpuCore::package_id` 推导得到——
但直接存储以避免在热路径上进行两步查找。

`CpuCore::numa_node` 与 `LogicalCpu::numa_node` 是设备级 NUMA 归属信息，
直接从 sysfs 读取，不依赖任何拓扑关系构建过程。

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
    std::optional<NumaNodeId> nearest_numa_node;  // 直接从 sysfs 读取
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

`devices` 是数据真源；便利方法均为非持有型的过滤查询。

`AcceleratorDevice::nearest_numa_node` 是设备级 NUMA 归属信息，
直接从 sysfs 读取，不依赖任何拓扑关系构建过程。

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
    std::optional<NumaNodeId> numa_node;   // 从 sysfs 直接读取
};

struct PciSubsystem
{
    std::vector<PciDevice> devices;

    const PciDevice* find(PciAddress addr) const;
};
```

`PciSubsystem` 是设备清单（"存在什么"）。
`PciDevice::numa_node` 是设备级 NUMA 归属信息，从 sysfs 直接读取，便于直接访问。

## Storage

```cpp
struct StorageDevice
{
    StorageId id;
    DeviceName name;
    std::optional<MemorySize> capacity;
    std::optional<PciAddress> pci_address;
    StorageKind kind;                     // Nvme, Sata, Sas 等
};

struct StorageSubsystem
{
    std::vector<StorageDevice> devices;
};
```

在 v0.0.1 中，Storage 仅提供基本的设备清单信息。
