/// @file hwloc_backend.cpp
/// @brief hwloc 拓扑后端实现
/// @details 在启用 SYSAL_HAVE_HWLOC 时，使用 hwloc API 采集 NUMA 节点本地
///          内存与 PCI 设备到最近 NUMA 节点的位置关系，构建 TopologyInfo；
///          未启用 hwloc 时该函数恒返回 std::nullopt。

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

/// @brief 查找 hwloc 对象最近的 NUMA 节点
/// @details 自当前对象起沿父节点链向上查找，逐层检查其 memory_first_child
///          子节点中是否存在 NUMANODE 类型节点，找到第一个即返回其 os_index。
/// @param obj 起始查找的 hwloc 对象
/// @return 找到返回对应 NumaNodeId，否则返回 std::nullopt
std::optional<NumaNodeId> find_nearest_numa_node(hwloc_obj_t obj)
{
    for(hwloc_obj_t ancestor = obj; ancestor != nullptr; ancestor = ancestor->parent)
    {
        // 遍历当前层级的 memory 子节点，查找 NUMANODE 类型
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

/// @brief 通过 hwloc 解析系统拓扑信息
/// @details 启用 hwloc 时：初始化拓扑并保留全部 PCI 设备，加载后分别遍历
///          NUMA 节点（记录 os_index 与本地内存大小）和 PCI 设备（记录地址与
///          最近 NUMA 节点）。任一初始化步骤失败均记录警告并提前返回。
///          若最终 NUMA 关系与设备位置均为空则视为无可用拓扑返回 std::nullopt。
///          未启用 hwloc 时直接返回 std::nullopt。
/// @param diag 诊断信息收集器，失败时写入警告
/// @return 成功且非空返回 TopologyInfo，否则返回 std::nullopt
std::optional<TopologyInfo> parse_topology_hwloc(Diagnostics& diag)
{
#ifdef SYSAL_HAVE_HWLOC
    hwloc_topology_t topology{};
    if(hwloc_topology_init(&topology) != 0)
    {
        add_warning(diag, "hwloc_topology_init failed");
        return std::nullopt;
    }

    // 保留全部 PCI 设备，避免默认过滤丢弃所需设备
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

    // 遍历 NUMA 节点，记录节点编号与本地内存
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

    // 遍历 PCI 设备，记录地址与最近 NUMA 节点
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

    // 无任何拓扑信息则视为未采集
    if(info.numa_relations.empty() && info.device_localities.empty())
    {
        return std::nullopt;
    }
    return info;
#else
    (void)diag;
    return std::nullopt;
#endif
}

} // namespace sysal::detail
