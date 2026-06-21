#include "memory_parser.hpp"
#include "parse_utils.hpp"
#include "parsed_facts.hpp"

#include "sysal/diagnostics.hpp"
#include "sysal/ids.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/units.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sysal::detail
{

namespace
{

std::optional<NumaMemoryInfo> parse_numa_meminfo(const std::string& content, std::uint32_t node_id)
{
    std::optional<MemorySize> total;
    std::optional<MemorySize> available;

    auto lines = split(content, '\n');
    for(const auto& line : lines)
    {
        auto [key, value] = split_kv(line);
        if(key.ends_with("MemTotal"))
        {
            auto bytes = extract_kb(value);
            if(bytes)
            {
                total = MemorySize{*bytes};
            }
        }
        else if(key.ends_with("MemFree"))
        {
            auto bytes = extract_kb(value);
            if(bytes)
            {
                available = MemorySize{*bytes};
            }
        }
    }

    if(!total)
    {
        return std::nullopt;
    }

    NumaMemoryInfo info;
    info.node = NumaNodeId(node_id);
    info.total = *total;
    info.available = available;
    return info;
}

std::vector<NumaMemoryInfo> parse_numa_memory(const RawStore& raw, Diagnostics& diag)
{
    auto path_map = build_path_map(raw, RawSource::SysfsNuma);
    auto node_keys = extract_prefix_keys(path_map, kNodePrefix);
    if(node_keys.empty())
    {
        return {};
    }

    std::vector<NumaMemoryInfo> result;
    for(const auto& key : node_keys)
    {
        auto node_id = node_id_from_key(key);
        if(!node_id)
        {
            add_warning(diag, "Cannot parse NUMA node id from: " + key, RawSource::SysfsNuma);
            continue;
        }

        auto base = std::string(kNodePrefix) + key + "/meminfo";
        auto meminfo_it = path_map.find(base);
        if(meminfo_it == path_map.end())
        {
            continue;
        }

        auto info = parse_numa_meminfo(meminfo_it->second, *node_id);
        if(info)
        {
            result.push_back(*info);
        }
    }
    return result;
}

} // namespace

std::optional<MemorySubsystem> parse_memory(const RawStore& raw, Diagnostics& diag)
{
    auto records = raw.get_all(RawSource::ProcMemInfo);
    if(records.empty())
    {
        return std::nullopt;
    }

    const auto* meminfo_record = records[0];
    if(meminfo_record->payload.empty())
    {
        add_warning(diag, "No /proc/meminfo data", RawSource::ProcMemInfo);
        return std::nullopt;
    }

    MemorySubsystem facts;
    bool found_total = false;

    auto lines = split(meminfo_record->payload, '\n');
    for(const auto& line : lines)
    {
        auto [key, value] = split_kv(line);
        if(key == "MemTotal")
        {
            auto bytes = extract_kb(value);
            if(bytes)
            {
                facts.total_memory = MemorySize{*bytes};
                found_total = true;
            }
        }
        else if(key == "MemAvailable")
        {
            auto bytes = extract_kb(value);
            if(bytes)
            {
                facts.available_memory = MemorySize{*bytes};
            }
        }
    }

    if(!found_total)
    {
        add_warning(diag, "MemTotal not found in /proc/meminfo", RawSource::ProcMemInfo);
        return std::nullopt;
    }

    facts.numa_memory = parse_numa_memory(raw, diag);

    return facts;
}

} // namespace sysal::detail
