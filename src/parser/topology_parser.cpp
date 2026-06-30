/// @file topology_parser.cpp
/// @brief 拓扑信息解析器实现
/// @details 解析系统 NUMA 拓扑与 PCI 设备的 NUMA 亲和关系。
///          优先使用 hwloc 后端；不可用时回退到 sysfs：
///          - /sys/devices/system/node/nodeN/ 解析 NUMA 节点及其本地内存；
///          - /sys/bus/pci/devices/<addr>/numa_node 解析 PCI 设备的最近 NUMA 节点。

#include "topology_parser.hpp"
#include "parse_utils.hpp"
#include "parsed_facts.hpp"

#include "../backend/hwloc_backend.hpp"

#include "sysal/diagnostics.hpp"
#include "sysal/ids.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/topology_info.hpp"
#include "sysal/units.hpp"
#include "sysal/value_types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sysal::detail
{

namespace
{

/// @brief sysfs PCI 设备目录前缀
/// @details 形如 /sys/bus/pci/devices/0000:01:00.0/numa_node。
constexpr std::string_view kPciPrefix = "/sys/bus/pci/devices/";

/// @brief 从 NUMA 节点 meminfo 内容中解析本地内存总量
/// @param content /sys/devices/system/node/nodeN/meminfo 的内容
/// @return 解析到的本地内存字节数；未找到 MemTotal 行时返回 std::nullopt
/// @details 在多行 meminfo 中查找以 "MemTotal" 结尾的键，提取其 KB 值并换算为字节。
std::optional<MemorySize> parse_node_mem_total(const std::string& content)
{
    auto lines = split(content, '\n');
    for(const auto& line : lines)
    {
        auto [key, value] = split_kv(line);
        if(key.ends_with("MemTotal"))
        {
            auto bytes = extract_kb(value);
            if(bytes)
            {
                return MemorySize{*bytes};
            }
        }
    }
    return std::nullopt;
}

/// @brief 从 sysfs 解析 NUMA 节点关系列表
/// @param raw 原始数据存储
/// @param diag 诊断信息容器
/// @return NUMA 节点关系列表；无节点数据时返回空
/// @details 流程：
///          1. 构建 sysfs NUMA 来源的路径映射；
///          2. 提取 /sys/devices/system/node/ 下的节点目录键（如 node0、node1）；
///          3. 逐节点解析 nodeN/meminfo（本地内存）与 nodeN/cplist；
///          4. 至少需要 cpulist 或 meminfo 之一，否则记录告警并跳过。
std::vector<NumaRelation> parse_numa_from_sysfs(const RawStore& raw, Diagnostics& diag)
{
    auto path_map = build_path_map(raw, RawSource::SysfsNuma);
    auto node_keys = extract_prefix_keys(path_map, kNodePrefix);
    if(node_keys.empty())
    {
        return {};
    }

    std::vector<NumaRelation> relations;
    for(const auto& key : node_keys)
    {
        // 从 "node0" 这样的键中提取节点编号
        auto node_id = node_id_from_key(key);
        if(!node_id)
        {
            add_warning(diag, "Cannot parse NUMA node id from: " + key, RawSource::SysfsNuma);
            continue;
        }

        auto base = std::string(kNodePrefix) + key + "/";

        // 解析该节点的本地内存总量
        auto meminfo_it = path_map.find(base + "meminfo");
        std::optional<MemorySize> local_memory;
        if(meminfo_it != path_map.end())
        {
            local_memory = parse_node_mem_total(meminfo_it->second);
        }

        // cpulist 与 meminfo 至少需要一个，否则该节点信息无效
        auto cpulist_it = path_map.find(base + "cpulist");
        if(cpulist_it == path_map.end() && !local_memory)
        {
            add_warning(diag, "NUMA node " + key + " has no cpulist or meminfo",
                        RawSource::SysfsNuma);
            continue;
        }

        NumaRelation rel;
        rel.node = NumaNodeId(*node_id);
        rel.local_memory = local_memory;
        relations.push_back(rel);
    }
    return relations;
}

/// @brief 从 sysfs 解析 PCI 设备的 NUMA 亲和关系
/// @param raw 原始数据存储
/// @param diag 诊断信息容器
/// @return 设备亲和关系列表；无 PCI 设备数据时返回空
/// @details 遍历 /sys/bus/pci/devices/<addr>/，读取 numa_node 文件。
///          跳过 numa_node 为负值（表示无 NUMA 亲和）的设备；
///          解析失败的设备会记录告警。
std::vector<DeviceLocality> parse_pci_localities_from_sysfs(const RawStore& raw, Diagnostics& diag)
{
    auto path_map = build_path_map(raw, RawSource::SysfsPci);
    auto device_keys = extract_prefix_keys(path_map, kPciPrefix);
    if(device_keys.empty())
    {
        return {};
    }

    std::vector<DeviceLocality> localities;
    for(const auto& dev_addr : device_keys)
    {
        auto base = std::string(kPciPrefix) + dev_addr + "/";
        auto numa_it = path_map.find(base + "numa_node");
        if(numa_it == path_map.end())
        {
            // 没有 numa_node 文件则跳过该设备
            continue;
        }

        auto node_str = trim(numa_it->second);
        // 负值表示设备未绑定到具体 NUMA 节点，跳过
        if(node_str.starts_with('-'))
        {
            continue;
        }
        auto node_val = parse_uint(node_str);
        if(!node_val)
        {
            add_warning(diag, "Cannot parse numa_node for PCI device " + dev_addr,
                        RawSource::SysfsPci);
            continue;
        }

        DeviceLocality loc;
        loc.pci_address = parse_pci_address(dev_addr);
        loc.nearest_numa_node = NumaNodeId(static_cast<std::uint32_t>(*node_val));
        localities.push_back(loc);
    }
    return localities;
}

} // namespace

/// @brief 从 RawStore 解析拓扑信息
/// @param raw 原始数据存储
/// @param diag 诊断信息容器，用于记录解析过程中的告警
/// @return 解析成功返回 TopologyInfo；无法获取任何拓扑数据时返回 std::nullopt
/// @details 优先尝试 hwloc 后端；若 hwloc 返回有效结果则直接采用，
///          否则回退到 sysfs 解析 NUMA 关系与 PCI 设备亲和。
///          两条 sysfs 路径都为空时返回 std::nullopt。
std::optional<TopologyInfo> parse_topology(const RawStore& raw, Diagnostics& diag)
{
    // 优先使用 hwloc 后端
    if(auto hwloc_result = parse_topology_hwloc(diag))
    {
        return hwloc_result;
    }

    // 回退：从 sysfs 解析 NUMA 与 PCI 亲和
    auto numa_relations = parse_numa_from_sysfs(raw, diag);
    auto device_localities = parse_pci_localities_from_sysfs(raw, diag);

    // 两者都为空则无拓扑信息
    if(numa_relations.empty() && device_localities.empty())
    {
        return std::nullopt;
    }

    TopologyInfo info;
    info.numa_relations = std::move(numa_relations);
    info.device_localities = std::move(device_localities);
    return info;
}

} // namespace sysal::detail
