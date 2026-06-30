/// @file cpu_parser.cpp
/// @brief CPU 子系统解析器实现
/// @details 从 /proc/cpuinfo 原始记录解析出 CPU 物理包、物理核与逻辑 CPU 的层级
///          结构，以及指令集扩展信息。

#include "cpu_parser.hpp"
#include "parse_utils.hpp"
#include "parsed_facts.hpp"

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sysal::detail
{

namespace
{

/// @brief /proc/cpuinfo 中单个处理器条目的中间表示
/// @details 在解析阶段逐行填充，用于后续构建 CpuSubsystem 层级结构。
///          has_physical_id / has_core_id 标记对应字段是否在 cpuinfo 中出现，
///          未出现时需回退到默认值（0 或 processor 编号）。
struct CpuInfoEntry
{
    std::uint32_t processor{};
    std::uint32_t physical_id{};
    std::uint32_t core_id{};
    std::string model_name;
    std::string vendor_id;
    std::string flags;
    bool has_physical_id{};
    bool has_core_id{};
};

/// @brief 解析 /proc/cpuinfo 的完整文本内容
/// @param content /proc/cpuinfo 的原始文本
/// @return 所有处理器条目列表；cpuinfo 中每个处理器之间以空行分隔
std::vector<CpuInfoEntry> parse_cpuinfo_content(const std::string& content)
{
    std::vector<CpuInfoEntry> entries;
    CpuInfoEntry current;
    bool has_entry = false;

    auto lines = split(content, '\n');
    for(const auto& line : lines)
    {
        // 空行表示一个处理器条目结束，提交当前条目并重置状态
        if(line.empty())
        {
            if(has_entry)
            {
                entries.push_back(current);
                current = {};
                has_entry = false;
            }
            continue;
        }

        auto [key, value] = split_kv(line);
        if(key.empty())
        {
            continue;
        }

        has_entry = true;
        if(key == "processor")
        {
            current.processor = static_cast<std::uint32_t>(parse_uint(value).value_or(0));
        }
        else if(key == "physical id")
        {
            current.physical_id = static_cast<std::uint32_t>(parse_uint(value).value_or(0));
            current.has_physical_id = true;
        }
        else if(key == "core id")
        {
            current.core_id = static_cast<std::uint32_t>(parse_uint(value).value_or(0));
            current.has_core_id = true;
        }
        else if(key == "model name")
        {
            current.model_name = value;
        }
        else if(key == "vendor_id")
        {
            current.vendor_id = value;
        }
        else if(key == "flags")
        {
            current.flags = value;
        }
    }

    // 文件末尾可能没有空行收尾，需补充提交最后一个条目
    if(has_entry)
    {
        entries.push_back(current);
    }

    return entries;
}

/// @brief 将 cpuinfo 的 flags 字段解析为 ISA 扩展枚举列表
/// @param flags /proc/cpuinfo 中 flags 行的内容（以空格分隔的 token 列表）
/// @return 识别到的 ISA 扩展列表，仅保留 sysal 关注的扩展（SSE4.2、AVX 系列）
std::vector<IsaExtension> parse_flags(const std::string& flags)
{
    std::vector<IsaExtension> result;
    auto tokens = split(flags, ' ');
    for(const auto& token : tokens)
    {
        if(token.empty())
        {
            continue;
        }
        if(token == "sse4_2")
        {
            result.push_back(IsaExtension::Sse42);
        }
        else if(token == "avx")
        {
            result.push_back(IsaExtension::Avx);
        }
        else if(token == "avx2")
        {
            result.push_back(IsaExtension::Avx2);
        }
        else if(token == "avx512f")
        {
            result.push_back(IsaExtension::Avx512f);
        }
        else if(token == "avx512cd")
        {
            result.push_back(IsaExtension::Avx512cd);
        }
        else if(token == "avx512bw")
        {
            result.push_back(IsaExtension::Avx512bw);
        }
        else if(token == "avx512dq")
        {
            result.push_back(IsaExtension::Avx512dq);
        }
        else if(token == "avx512vl")
        {
            result.push_back(IsaExtension::Avx512vl);
        }
    }
    return result;
}

/// @brief 从 uname 原始记录推断 CPU 架构
/// @param raw 原始数据存储
/// @return 解析到的架构枚举；找不到 uname 记录时返回 Architecture::Other
Architecture determine_arch(const RawStore& raw)
{
    auto records = raw.get_all(RawSource::ProcUname);
    for(const auto* record : records)
    {
        if(record->path_or_command == "uname")
        {
            auto parts = split(record->payload, ' ');
            // uname 输出最后一字段为 machine 架构标识（如 x86_64、aarch64）
            if(!parts.empty())
            {
                return arch_from_machine(parts.back());
            }
        }
    }
    return Architecture::Other;
}

} // namespace

std::optional<CpuSubsystem> parse_cpu(const RawStore& raw, Diagnostics& diag)
{
    auto records = raw.get_all(RawSource::ProcCpuInfo);
    if(records.empty())
    {
        return std::nullopt;
    }

    const auto* cpuinfo_record = records[0];
    if(cpuinfo_record->payload.empty())
    {
        add_warning(diag, "No /proc/cpuinfo data", RawSource::ProcCpuInfo);
        return std::nullopt;
    }

    auto entries = parse_cpuinfo_content(cpuinfo_record->payload);
    if(entries.empty())
    {
        add_warning(diag, "No CPU entries in /proc/cpuinfo", RawSource::ProcCpuInfo);
        return std::nullopt;
    }

    CpuSubsystem facts;
    facts.arch = determine_arch(raw);

    // 收集去重后的物理包 ID 与物理核键 (package_id, core_id)
    std::vector<std::uint32_t> package_ids;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> core_keys;

    for(const auto& entry : entries)
    {
        // 缺少 physical id 时回退到 0（单包系统）
        auto pkg_id = entry.has_physical_id ? entry.physical_id : 0U;
        // 缺少 core id 时回退到 processor 编号（视为每核单线程）
        auto core_id_val = entry.has_core_id ? entry.core_id : entry.processor;

        if(std::find(package_ids.begin(), package_ids.end(), pkg_id) == package_ids.end())
        {
            package_ids.push_back(pkg_id);
        }

        auto core_key = std::make_pair(pkg_id, core_id_val);
        if(std::find(core_keys.begin(), core_keys.end(), core_key) == core_keys.end())
        {
            core_keys.push_back(core_key);
        }
    }

    // 排序使生成的 ID 稳定且可复现
    std::sort(package_ids.begin(), package_ids.end());
    std::sort(core_keys.begin(), core_keys.end());

    // 构建 CPU 物理包列表，从首个匹配条目提取厂商与型号名
    for(auto pkg_id : package_ids)
    {
        CpuPackage pkg;
        pkg.id = CpuPackageId(pkg_id);
        for(const auto& entry : entries)
        {
            auto entry_pkg = entry.has_physical_id ? entry.physical_id : 0U;
            if(entry_pkg == pkg_id)
            {
                pkg.vendor = Vendor{entry.vendor_id};
                pkg.model_name = DeviceName{entry.model_name};
                break;
            }
        }
        facts.packages.push_back(pkg);
    }

    // 构建物理核列表，核 ID 为排序后 core_keys 中的下标
    for(std::size_t i = 0; i < core_keys.size(); ++i)
    {
        CpuCore core;
        core.id = CpuCoreId(static_cast<std::uint32_t>(i));
        core.package_id = CpuPackageId(core_keys[i].first);
        facts.cores.push_back(core);
    }

    // 构建逻辑 CPU 列表，建立与物理核的映射关系
    for(const auto& entry : entries)
    {
        auto pkg_id = entry.has_physical_id ? entry.physical_id : 0U;
        auto core_id_val = entry.has_core_id ? entry.core_id : entry.processor;
        auto core_key = std::make_pair(pkg_id, core_id_val);

        auto core_it = std::find(core_keys.begin(), core_keys.end(), core_key);
        auto core_idx = static_cast<std::uint32_t>(std::distance(core_keys.begin(), core_it));

        LogicalCpu cpu;
        cpu.id = LogicalCpuId(entry.processor);
        cpu.core_id = CpuCoreId(core_idx);
        cpu.package_id = CpuPackageId(pkg_id);
        facts.logical_cpus.push_back(cpu);
    }

    // 统计每个物理核上的逻辑线程数
    for(auto& core : facts.cores)
    {
        for(const auto& cpu : facts.logical_cpus)
        {
            if(cpu.core_id == core.id)
            {
                ++core.logical_threads;
            }
        }
    }

    // 统计每个物理包的物理核数与逻辑线程数
    for(auto& pkg : facts.packages)
    {
        for(const auto& core : facts.cores)
        {
            if(core.package_id == pkg.id)
            {
                ++pkg.physical_cores;
                pkg.logical_threads += core.logical_threads;
            }
        }
    }

    // ISA 扩展来自第一个处理器条目的 flags（同包内各核一致）
    facts.isa_extensions = parse_flags(entries[0].flags);

    return facts;
}

} // namespace sysal::detail
