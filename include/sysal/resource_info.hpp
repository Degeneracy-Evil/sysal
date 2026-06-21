#pragma once

#include "sysal/enums.hpp"
#include "sysal/ids.hpp"
#include "sysal/topology_info.hpp"
#include "sysal/units.hpp"
#include "sysal/value_types.hpp"

#include <optional>
#include <vector>

namespace sysal
{

struct NumaNode
{
    NumaNodeId id;
    std::optional<MemorySize> local_memory;
};

struct CpuPackage
{
    CpuPackageId id;
    Vendor vendor;
    DeviceName model_name;
    std::uint32_t physical_cores{};
    std::uint32_t logical_threads{};
    std::optional<Frequency> base_frequency;
    std::optional<Frequency> max_frequency;
};

struct CpuCore
{
    CpuCoreId id;
    CpuPackageId package_id;
    std::uint32_t logical_threads{};
    std::optional<NumaNodeId> numa_node;
};

struct LogicalCpu
{
    LogicalCpuId id;
    CpuCoreId core_id;
    CpuPackageId package_id;
    std::optional<NumaNodeId> numa_node;
    bool visible_to_current_process{};
};

struct CpuSubsystem
{
    Architecture arch{};
    std::vector<CpuPackage> packages;
    std::vector<CpuCore> cores;
    std::vector<LogicalCpu> logical_cpus;
    std::vector<NumaNode> numa_nodes;
    std::vector<IsaExtension> isa_extensions;

    const CpuPackage* find_package(CpuPackageId id) const;
    const CpuCore* find_core(CpuCoreId id) const;
    const LogicalCpu* find_logical_cpu(LogicalCpuId id) const;
    std::vector<const LogicalCpu*> logical_cpus_of_package(CpuPackageId id) const;
    std::vector<const LogicalCpu*> logical_cpus_of_core(CpuCoreId id) const;
    std::vector<const CpuCore*> cores_of_package(CpuPackageId id) const;
    std::vector<const LogicalCpu*> visible_logical_cpus() const;
};

struct NumaMemoryInfo
{
    NumaNodeId node;
    MemorySize total;
    std::optional<MemorySize> available;
};

struct MemorySubsystem
{
    MemorySize total_memory;
    std::optional<MemorySize> available_memory;
    std::vector<NumaMemoryInfo> numa_memory;
};

struct AcceleratorDevice
{
    AcceleratorId id;
    AcceleratorKind kind{};
    Vendor vendor;
    DeviceName name;
    std::optional<PciAddress> pci_address;
    std::optional<NumaNodeId> nearest_numa_node;
    std::optional<MemorySize> memory_size;
    std::optional<DriverId> driver;
    bool visible_to_current_process{};
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

struct NetworkInterface
{
    InterfaceName name;
    MacAddress mac;
    InterfaceState state{};
    std::optional<Bandwidth> speed;
    std::vector<IpAddress> addresses;
    std::optional<PciAddress> pci_address;
    std::optional<RdmaDeviceId> rdma_device;
    bool visible_to_current_process{};
};

struct NetworkSubsystem
{
    std::vector<NetworkInterface> interfaces;

    std::vector<const NetworkInterface*> visible() const;
    const NetworkInterface* find(const InterfaceName& name) const;
};

struct PciDevice
{
    PciAddress address;
    Vendor vendor;
    DeviceName device_name;
    std::string device_class;
    std::optional<NumaNodeId> numa_node;
};

struct PciSubsystem
{
    std::vector<PciDevice> devices;

    const PciDevice* find(PciAddress addr) const;
};

struct StorageDevice
{
    StorageId id;
    DeviceName name;
    std::optional<MemorySize> capacity;
    std::optional<PciAddress> pci_address;
    StorageKind kind{};
};

struct StorageSubsystem
{
    std::vector<StorageDevice> devices;
};

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

} // namespace sysal
