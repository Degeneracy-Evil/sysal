#include "hwloc_backend.hpp"

#include "../parser/parse_utils.hpp"

#include "sysal/diagnostics.hpp"
#include "sysal/ids.hpp"
#include "sysal/topology_info.hpp"
#include "sysal/units.hpp"
#include "sysal/value_types.hpp"

#include <optional>

#ifdef SYSAL_HAVE_HWLOC
    #include <hwloc.h>
#endif

namespace sysal::detail
{

#ifdef SYSAL_HAVE_HWLOC

namespace
{

std::optional<NumaNodeId> find_nearest_numa_node(hwloc_obj_t obj)
{
    for(hwloc_obj_t ancestor = obj; ancestor != nullptr; ancestor = ancestor->parent)
    {
        for(hwloc_obj_t mem = ancestor->memory_first_child; mem != nullptr; mem = mem->next_sibling)
        {
            if(mem->type == HWLOC_OBJ_NUMANODE)
            {
                return NumaNodeId(mem->os_index);
            }
        }
    }
    return std::nullopt;
}

} // namespace

#endif

std::optional<TopologyInfo> parse_topology_hwloc(Diagnostics& diag)
{
#ifdef SYSAL_HAVE_HWLOC
    hwloc_topology_t topology{};
    if(hwloc_topology_init(&topology) != 0)
    {
        add_warning(diag, "hwloc_topology_init failed");
        return std::nullopt;
    }

    if(hwloc_topology_set_type_filter(topology, HWLOC_OBJ_PCI_DEVICE, HWLOC_TYPE_FILTER_KEEP_ALL) !=
       0)
    {
        add_warning(diag, "hwloc set PCI device filter failed");
        hwloc_topology_destroy(topology);
        return std::nullopt;
    }

    if(hwloc_topology_load(topology) != 0)
    {
        add_warning(diag, "hwloc_topology_load failed");
        hwloc_topology_destroy(topology);
        return std::nullopt;
    }

    TopologyInfo info;

    int numa_depth = hwloc_get_type_depth(topology, HWLOC_OBJ_NUMANODE);
    if(numa_depth != HWLOC_TYPE_DEPTH_UNKNOWN && numa_depth >= 0)
    {
        unsigned count = hwloc_get_nbobjs_by_depth(topology, numa_depth);
        for(unsigned i = 0; i < count; ++i)
        {
            hwloc_obj_t obj = hwloc_get_obj_by_depth(topology, numa_depth, i);
            if(obj == nullptr || obj->attr == nullptr)
            {
                continue;
            }
            NumaRelation rel;
            rel.node = NumaNodeId(obj->os_index);
            rel.local_memory = MemorySize{obj->attr->numanode.local_memory};
            info.numa_relations.push_back(rel);
        }
    }

    int pci_depth = hwloc_get_type_depth(topology, HWLOC_OBJ_PCI_DEVICE);
    if(pci_depth != HWLOC_TYPE_DEPTH_UNKNOWN && pci_depth >= 0)
    {
        unsigned count = hwloc_get_nbobjs_by_depth(topology, pci_depth);
        for(unsigned i = 0; i < count; ++i)
        {
            hwloc_obj_t obj = hwloc_get_obj_by_depth(topology, pci_depth, i);
            if(obj == nullptr || obj->attr == nullptr)
            {
                continue;
            }
            auto nearest = find_nearest_numa_node(obj);
            if(!nearest)
            {
                continue;
            }
            DeviceLocality loc;
            loc.pci_address = PciAddress{
                .domain = static_cast<std::uint16_t>(obj->attr->pcidev.domain),
                .bus = obj->attr->pcidev.bus,
                .device = obj->attr->pcidev.dev,
                .function = obj->attr->pcidev.func,
            };
            loc.nearest_numa_node = *nearest;
            info.device_localities.push_back(loc);
        }
    }

    hwloc_topology_destroy(topology);

    if(info.numa_relations.empty() && info.device_localities.empty())
    {
        return std::nullopt;
    }
    return info;
#else
    return std::nullopt;
#endif
}

} // namespace sysal::detail
