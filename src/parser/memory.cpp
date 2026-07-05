/// @file memory.cpp
/// @brief 内存解析器实现
/// @details 从 /proc/meminfo 和 sysfs NUMA meminfo 解析内存总量和 NUMA 内存分布。

#include "memory.hpp"

#include "parse_utils.hpp"

#include <map>
#include <set>
#include <string_view>

namespace sysal::detail
{

namespace
{

/// @brief 解析 /proc/meminfo 内容
/// @param payload /proc/meminfo 文件内容
/// @param warnings 警告列表
/// @return pair<total_memory, available_memory>
std::pair<MemorySize, std::optional<MemorySize>> parse_meminfo(std::string_view payload,
                                                               std::vector<std::string>& warnings)
{
    MemorySize total{0};
    std::optional<MemorySize> available;

    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto [key, value] = parse_kv(line, ':');
        if(key == "MemTotal")
        {
            // 值格式: "395617072 kB" — 提取数字部分
            auto parts = split(value, ' ');
            if(!parts.empty())
            {
                auto kb = parse_kb_to_bytes(trim(parts[0]));
                if(kb.has_value())
                {
                    total = *kb;
                }
                else
                {
                    warnings.push_back("parse_memory: MemTotal 值解析失败: " + std::string(value));
                }
            }
        }
        else if(key == "MemAvailable")
        {
            auto parts = split(value, ' ');
            if(!parts.empty())
            {
                auto kb = parse_kb_to_bytes(trim(parts[0]));
                if(kb.has_value())
                {
                    available = *kb;
                }
                else
                {
                    warnings.push_back("parse_memory: MemAvailable 值解析失败: " +
                                       std::string(value));
                }
            }
        }
    }

    return {total, available};
}

/// @brief 解析 NUMA 节点 meminfo 内容
/// @param payload 单个 NUMA 节点的 meminfo 内容
/// @param node_id NUMA 节点 ID
/// @param warnings 警告列表
/// @return NumaMemory 结构体
NumaMemory parse_numa_meminfo(std::string_view payload, std::uint32_t node_id,
                              std::vector<std::string>& warnings)
{
    NumaMemory nm;
    nm.node = NumaNodeId{node_id};
    nm.total = MemorySize{0};

    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto [key, value] = parse_kv(line, ':');
        // 格式: "Node N Total" 或 "Node N Free"
        auto trimmed_key = trim(key);

        // 检查是否为 "Node N Total" 格式
        if(trimmed_key.find("Total") != std::string::npos)
        {
            auto parts = split(value, ' ');
            if(!parts.empty())
            {
                auto kb = parse_kb_to_bytes(trim(parts[0]));
                if(kb.has_value())
                {
                    nm.total = *kb;
                }
                else
                {
                    warnings.push_back("parse_numa_meminfo: node " + std::to_string(node_id) +
                                       " Total 值解析失败: " + std::string(value));
                }
            }
        }
        else if(trimmed_key.find("Free") != std::string::npos)
        {
            auto parts = split(value, ' ');
            if(!parts.empty())
            {
                auto kb = parse_kb_to_bytes(trim(parts[0]));
                if(kb.has_value())
                {
                    nm.available = *kb;
                }
                else
                {
                    warnings.push_back("parse_numa_meminfo: node " + std::to_string(node_id) +
                                       " Free 值解析失败: " + std::string(value));
                }
            }
        }
    }

    return nm;
}

/// @brief 从 udevadm 输出解析 DIMM 信息
/// @param payload udevadm info -e 输出
/// @param warnings 警告列表
/// @return DIMM 列表（按索引排序）
std::vector<DimmInfo> parse_udevadm_dimms(std::string_view payload,
                                          std::vector<std::string>& warnings)
{
    constexpr std::string_view prefix = "MEMORY_DEVICE_";
    std::map<std::uint32_t, DimmInfo> dimm_map;
    std::set<std::uint32_t> present_seen;

    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto trimmed = trim(line);
        if(trimmed.empty() || trimmed[0] != 'E')
        {
            continue;
        }
        // E: KEY=VALUE
        auto [tag, rest] = parse_kv(trimmed, ':');
        if(tag != "E")
        {
            warnings.push_back("parse_udevadm_dimms: 非 E 标签: " + trimmed);
            continue;
        }
        if(rest.empty())
        {
            warnings.push_back("parse_udevadm_dimms: E: 值为空: " + trimmed);
            continue;
        }
        auto eq_pos = rest.find('=');
        if(eq_pos == std::string::npos)
        {
            continue;
        }
        auto key = trim(rest.substr(0, eq_pos));
        auto value = trim(rest.substr(eq_pos + 1));

        if(key.size() <= prefix.size() || key.substr(0, prefix.size()) != prefix)
        {
            continue;
        }
        auto after_prefix = key.substr(prefix.size());
        auto us_pos = after_prefix.find('_');
        if(us_pos == std::string::npos)
        {
            warnings.push_back("parse_udevadm_dimms: key 前缀后无下划线: " + key);
            continue;
        }
        auto idx_str = after_prefix.substr(0, us_pos);
        auto field = after_prefix.substr(us_pos + 1);
        if(field.empty())
        {
            warnings.push_back("parse_udevadm_dimms: key 下划线后无字段: " + key);
            continue;
        }
        auto idx = parse_uint(idx_str);
        if(!idx.has_value())
        {
            warnings.push_back("parse_udevadm_dimms: 索引解析失败: " + key);
            continue;
        }

        auto idx32 = static_cast<std::uint32_t>(*idx);
        auto& dimm = dimm_map[idx32];

        if(field == "TYPE")
        {
            dimm.type = value;
        }
        else if(field == "SPEED_MTS")
        {
            auto v = parse_uint(value);
            if(v.has_value())
            {
                dimm.speed_mts = TransferRate{*v};
            }
        }
        else if(field == "CONFIGURED_SPEED_MTS")
        {
            auto v = parse_uint(value);
            if(v.has_value())
            {
                dimm.configured_speed_mts = TransferRate{*v};
            }
        }
        else if(field == "SIZE")
        {
            auto v = parse_uint(value);
            if(v.has_value())
            {
                dimm.size = MemorySize{*v};
            }
        }
        else if(field == "MANUFACTURER")
        {
            dimm.manufacturer = Vendor{value};
        }
        else if(field == "PART_NUMBER")
        {
            dimm.part_number = value;
        }
        else if(field == "LOCATOR")
        {
            dimm.locator = value;
        }
        else if(field == "BANK_LOCATOR")
        {
            dimm.bank_locator = value;
        }
        else if(field == "RANK")
        {
            auto v = parse_uint(value);
            if(v.has_value())
            {
                dimm.rank = static_cast<std::uint32_t>(*v);
            }
        }
        else if(field == "TOTAL_WIDTH")
        {
            auto v = parse_uint(value);
            if(v.has_value())
            {
                dimm.total_width = static_cast<std::uint32_t>(*v);
            }
        }
        else if(field == "DATA_WIDTH")
        {
            auto v = parse_uint(value);
            if(v.has_value())
            {
                dimm.data_width = static_cast<std::uint32_t>(*v);
            }
        }
        else if(field == "FORM_FACTOR")
        {
            dimm.form_factor = value;
        }
        else if(field == "PRESENT")
        {
            present_seen.insert(idx32);
            dimm.present = (value == "1");
        }
    }

    std::vector<DimmInfo> result;
    result.reserve(dimm_map.size());
    for(auto& [idx, dimm] : dimm_map)
    {
        if(present_seen.count(idx) == 0)
        {
            dimm.present = (dimm.size.value > 0);
        }
        result.push_back(std::move(dimm));
    }
    return result;
}

/// @brief 从 EDAC sysfs 记录解析 DIMM 信息
/// @param edac_records SysfsEdac 成功记录指针列表
/// @return DIMM 列表
std::vector<DimmInfo> parse_edac_dimms(const std::vector<const RawRecord*>& edac_records)
{
    std::map<std::string, DimmInfo> grouped;
    for(const auto* rec : edac_records)
    {
        if(rec->status != CollectStatus::Success)
        {
            continue;
        }
        const auto& path = rec->path_or_command;
        auto slash = path.find_last_of('/');
        if(slash == std::string::npos)
        {
            continue;
        }
        auto dir = path.substr(0, slash);
        auto fname = path.substr(slash + 1);
        auto value = trim(rec->payload);

        auto& dimm = grouped[dir];
        dimm.present = true;

        if(fname == "dimm_mem_type")
        {
            dimm.type = value;
        }
        else if(fname == "size")
        {
            auto mb = parse_uint(value);
            if(mb.has_value())
            {
                dimm.size = MemorySize{*mb * 1024 * 1024};
            }
        }
        else if(fname == "dimm_label")
        {
            dimm.locator = value;
        }
        else if(fname == "dimm_location")
        {
            dimm.bank_locator = value;
        }
    }

    std::vector<DimmInfo> result;
    result.reserve(grouped.size());
    for(auto& [dir, dimm] : grouped)
    {
        result.push_back(std::move(dimm));
    }
    return result;
}

} // namespace

std::optional<Memory> parse_memory(const RawStore& raw, std::vector<std::string>& warnings)
{
    Memory memory;

    // 解析 /proc/meminfo
    auto meminfo_records = raw.get_all(RawSource::ProcMemInfo);
    const RawRecord* meminfo_rec = nullptr;
    for(const auto* rec : meminfo_records)
    {
        if(rec->status == CollectStatus::Success)
        {
            meminfo_rec = rec;
            break;
        }
    }
    if(meminfo_rec == nullptr)
    {
        warnings.push_back("parse_memory: 缺少 /proc/meminfo 数据");
        return std::nullopt;
    }

    auto [total, available] = parse_meminfo(meminfo_rec->payload, warnings);
    if(total.value == 0)
    {
        warnings.push_back("parse_memory: MemTotal 为 0 或未找到");
        return std::nullopt;
    }
    memory.total_memory = total;
    memory.available_memory = available;

    // 解析 NUMA 节点 meminfo
    auto numa_records = raw.get_all(RawSource::SysfsNuma);
    for(const auto* rec : numa_records)
    {
        if(rec->status != CollectStatus::Success)
        {
            continue;
        }

        const auto& path = rec->path_or_command;
        // 查找 meminfo: node/nodeN/meminfo
        if(path.find("meminfo") == std::string::npos)
        {
            continue;
        }

        // 从路径提取节点 ID: node/nodeN/meminfo
        // 查找 "node/node" 模式
        auto node_node_pos = path.find("node/node");
        if(node_node_pos == std::string::npos)
        {
            continue;
        }
        auto after_node_node = path.substr(node_node_pos + 9); // "node/node" 长度
        auto slash_pos = after_node_node.find('/');
        if(slash_pos == std::string::npos)
        {
            continue;
        }
        auto node_id_str = after_node_node.substr(0, slash_pos);
        auto node_id = parse_uint(node_id_str);
        if(!node_id.has_value())
        {
            continue;
        }

        auto nm = parse_numa_meminfo(rec->payload, static_cast<std::uint32_t>(*node_id), warnings);
        if(nm.total.value > 0)
        {
            memory.numa_memory.push_back(nm);
        }
    }

    // 解析 DIMM 信息：udevadm 优先，无则回退 EDAC
    auto udevadm_records = raw.get_all(RawSource::Udevadm);
    for(const auto* rec : udevadm_records)
    {
        if(rec->status != CollectStatus::Success)
        {
            continue;
        }
        auto dimms = parse_udevadm_dimms(rec->payload, warnings);
        if(!dimms.empty())
        {
            memory.dimms = std::move(dimms);
            break;
        }
    }

    if(memory.dimms.empty())
    {
        auto edac_records = raw.get_all(RawSource::SysfsEdac);
        memory.dimms = parse_edac_dimms(edac_records);
    }

    if(!memory.dimms.empty())
    {
        memory.dimm_count = static_cast<std::uint32_t>(memory.dimms.size());
        std::uint32_t populated = 0;
        for(const auto& d : memory.dimms)
        {
            if(d.present)
            {
                ++populated;
            }
        }
        memory.populated_dimms = populated;
    }

    return memory;
}

} // namespace sysal::detail
