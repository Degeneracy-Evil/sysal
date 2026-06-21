#pragma once

#include "sysal/ids.hpp"
#include "sysal/units.hpp"
#include "sysal/value_types.hpp"

#include <vector>

namespace sysal
{

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

struct TopologyInfo
{
    std::vector<NumaRelation> numa_relations;
    std::vector<PciRelation> pci_relations;
    std::vector<DeviceLocality> device_localities;
};

} // namespace sysal
