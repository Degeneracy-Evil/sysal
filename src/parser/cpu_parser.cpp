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

std::vector<CpuInfoEntry> parse_cpuinfo_content(const std::string& content)
{
    std::vector<CpuInfoEntry> entries;
    CpuInfoEntry current;
    bool has_entry = false;

    auto lines = split(content, '\n');
    for(const auto& line : lines)
    {
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

    if(has_entry)
    {
        entries.push_back(current);
    }

    return entries;
}

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

Architecture determine_arch(const RawStore& raw)
{
    auto records = raw.get_all(RawSource::ProcUname);
    for(const auto* record : records)
    {
        if(record->path_or_command == "uname")
        {
            auto parts = split(record->payload, ' ');
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

    std::vector<std::uint32_t> package_ids;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> core_keys;

    for(const auto& entry : entries)
    {
        auto pkg_id = entry.has_physical_id ? entry.physical_id : 0U;
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

    std::sort(package_ids.begin(), package_ids.end());
    std::sort(core_keys.begin(), core_keys.end());

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

    for(std::size_t i = 0; i < core_keys.size(); ++i)
    {
        CpuCore core;
        core.id = CpuCoreId(static_cast<std::uint32_t>(i));
        core.package_id = CpuPackageId(core_keys[i].first);
        facts.cores.push_back(core);
    }

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

    facts.isa_extensions = parse_flags(entries[0].flags);

    return facts;
}

} // namespace sysal::detail
