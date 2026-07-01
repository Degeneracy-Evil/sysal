/// @file memory.cpp
/// @brief 内存解析器实现
/// @details 从 /proc/meminfo 和 sysfs NUMA meminfo 解析内存总量和 NUMA 内存分布。

#include "memory.hpp"

#include "parse_utils.hpp"

#include <map>
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
                              [[maybe_unused]] std::vector<std::string>& warnings)
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
            }
        }
    }

    return nm;
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

    return memory;
}

} // namespace sysal::detail
