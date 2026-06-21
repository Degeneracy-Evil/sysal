# TopologyInfo

sysal's normalized representation of resource relationships.
Only stores references (IDs / addresses), never duplicates device attributes.

```cpp
struct TopologyInfo
{
    std::vector<NumaRelation> numa_relations;
    std::vector<PciRelation> pci_relations;
    std::vector<DeviceLocality> device_localities;
};

struct NumaRelation
{
    NumaNodeId node;
    std::vector<CpuPackageId> packages;
    std::optional<MemorySize> local_memory;
};

struct PciRelation
{
    PciAddress parent;
    PciAddress child;
};

struct DeviceLocality
{
    PciAddress pci_address;
    NumaNodeId nearest_numa_node;
};
```

## Relationship to PciSubsystem

1. `PciSubsystem::devices` is the sole source of device attributes (vendor, name, class).
2. `TopologyInfo` references devices by `PciAddress` only.
3. All `PciAddress` values in `TopologyInfo` must resolve in `PciSubsystem::devices`.
4. Resolver guarantees referential integrity; dangling references are recorded as Diagnostics.

## Backend

sysal may use `hwloc` as a backend provider for topology.
Backend-specific types (`hwloc_obj_t`, etc.) must not leak into the public API.

```txt
hwloc / sysfs / NVML / ibverbs
    ↓
sysal raw source
    ↓
sysal normalized model
```
