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

constexpr std::string_view kPciPrefix = "/sys/bus/pci/devices/";

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
        auto node_id = node_id_from_key(key);
        if(!node_id)
        {
            add_warning(diag, "Cannot parse NUMA node id from: " + key, RawSource::SysfsNuma);
            continue;
        }

        auto base = std::string(kNodePrefix) + key + "/";

        auto meminfo_it = path_map.find(base + "meminfo");
        std::optional<MemorySize> local_memory;
        if(meminfo_it != path_map.end())
        {
            local_memory = parse_node_mem_total(meminfo_it->second);
        }

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
            continue;
        }

        auto node_str = trim(numa_it->second);
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

std::optional<TopologyInfo> parse_topology(const RawStore& raw, Diagnostics& diag)
{
    if(auto hwloc_result = parse_topology_hwloc(diag))
    {
        return hwloc_result;
    }

    auto numa_relations = parse_numa_from_sysfs(raw, diag);
    auto device_localities = parse_pci_localities_from_sysfs(raw, diag);

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
