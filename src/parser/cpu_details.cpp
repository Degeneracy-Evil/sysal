#include "cpu_details.hpp"
#include "parse_utils.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <map>
#include <set>

namespace sysal::detail
{
    std::optional<std::vector<LogicalCpuId>> parse_cpu_id_list(std::string_view value)
    {
        constexpr std::size_t max_cpu_count = 65536;
        std::string normalized{value};
        for(auto &character : normalized)
        {
            if(std::isspace(static_cast<unsigned char>(character)) != 0)
                character = ',';
        }
        std::set<std::uint32_t> ids;
        for(const auto &token : split(normalized, ','))
        {
            if(token.empty())
                continue;
            const auto dash = token.find('-');
            const auto first = parse_uint(token.substr(0, dash));
            const auto last = dash == std::string::npos ? first : parse_uint(token.substr(dash + 1));
            if(!first || !last || *last < *first || *last > std::numeric_limits<std::uint32_t>::max() ||
               *last - *first >= max_cpu_count)
            {
                return std::nullopt;
            }
            const auto first_id = first.value();
            const auto last_id = last.value();
            for(auto id = first_id; id <= last_id; ++id)
            {
                ids.insert(static_cast<std::uint32_t>(id));
                if(ids.size() > max_cpu_count)
                    return std::nullopt;
            }
        }
        std::vector<LogicalCpuId> result;
        result.reserve(ids.size());
        for(auto id : ids)
            result.emplace_back(id);
        return result;
    }

    namespace
    {
        using FrequencyMember = std::optional<Frequency> CpuFrequencyPolicy::*;
        constexpr std::array<std::pair<std::string_view, FrequencyMember>, 7> frequency_fields{{
            {"base_frequency", &CpuFrequencyPolicy::base_frequency},
            {"cpuinfo_min_freq", &CpuFrequencyPolicy::hardware_min_frequency},
            {"cpuinfo_max_freq", &CpuFrequencyPolicy::hardware_max_frequency},
            {"scaling_min_freq", &CpuFrequencyPolicy::scaling_min_frequency},
            {"scaling_max_freq", &CpuFrequencyPolicy::scaling_max_frequency},
            {"scaling_cur_freq", &CpuFrequencyPolicy::scaling_current_frequency},
            {"cpuinfo_cur_freq", &CpuFrequencyPolicy::hardware_current_frequency},
        }};

        void read_policy_field(CpuFrequencyPolicy &policy, const RawRecord &record, std::vector<std::string> &warnings)
        {
            const auto field = extract_filename(record.path_or_command);
            if(field == "related_cpus" || field == "affected_cpus")
            {
                if(auto ids = parse_cpu_id_list(record.payload))
                {
                    (field == "related_cpus" ? policy.related_cpus : policy.affected_cpus) = std::move(*ids);
                }
                else
                    warnings.push_back("parse_cpu: invalid CPU list in " + record.path_or_command);
                return;
            }
            for(const auto &[name, member] : frequency_fields)
            {
                if(field != name)
                    continue;
                const auto number = parse_uint(record.payload);
                if(number && *number > 0 && *number <= std::numeric_limits<std::uint64_t>::max() / 1000)
                {
                    policy.*member = Frequency{*number * 1000};
                }
                else
                    warnings.push_back("parse_cpu: invalid frequency in " + record.path_or_command);
                return;
            }
            if(field == "scaling_driver")
                policy.driver = trim(record.payload);
            else if(field == "scaling_governor")
                policy.governor = trim(record.payload);
            else if(field == "energy_performance_preference")
                policy.energy_performance_preference = trim(record.payload);
        }
    } // namespace

    void read_cpu_details(const RawStore &raw, Cpu &cpu, std::vector<std::string> &warnings)
    {
        constexpr std::string_view base = "/sys/devices/system/cpu/";
        constexpr std::string_view policy_prefix = "/sys/devices/system/cpu/cpufreq/policy";
        constexpr std::string_view legacy_prefix = "/sys/devices/system/cpu/cpu";
        const auto records = raw.get_all(RawSource::SysfsCpu);
        const bool modern_policies = std::ranges::any_of(
            records, [policy_prefix](const auto *record)
            { return record->status == CollectStatus::Success && record->path_or_command.starts_with(policy_prefix); });
        std::map<std::uint32_t, bool> reported_online;
        std::map<std::uint32_t, CpuFrequencyPolicy> policies;
        for(const auto *record : records)
        {
            if(record->status != CollectStatus::Success)
                continue;
            const std::string_view path = record->path_or_command;
            if(path == std::string{base} + "present" || path == std::string{base} + "online")
            {
                const auto ids = parse_cpu_id_list(record->payload);
                if(path.ends_with("present"))
                    cpu.present_cpu_ids = ids;
                else
                    cpu.online_cpu_ids = ids;
                if(!ids)
                    warnings.push_back("parse_cpu: invalid CPU list in " + record->path_or_command);
            }
            else if(path == std::string{base} + "smt/control")
                cpu.smt_control = trim(record->payload);
            else if(path == std::string{base} + "smt/active" || path == std::string{base} + "cpufreq/boost")
            {
                const auto value = trim(record->payload);
                if(value == "0" || value == "1")
                {
                    (path.ends_with("active") ? cpu.smt_active : cpu.boost_enabled) = value == "1";
                }
            }
            else if(path.starts_with(legacy_prefix) && path.ends_with("/online"))
            {
                const auto suffix = path.substr(legacy_prefix.size());
                const auto number = parse_uint(suffix.substr(0, suffix.find('/')));
                const auto value = trim(record->payload);
                if(number && *number <= std::numeric_limits<std::uint32_t>::max() && (value == "0" || value == "1"))
                    reported_online[static_cast<std::uint32_t>(*number)] = value == "1";
            }
            else if(path.starts_with(policy_prefix) || (!modern_policies && path.starts_with(legacy_prefix)))
            {
                const bool legacy = !path.starts_with(policy_prefix);
                const auto suffix = path.substr(legacy ? legacy_prefix.size() : policy_prefix.size());
                const auto separator = suffix.find('/');
                if(separator == std::string_view::npos ||
                   (legacy && !suffix.substr(separator).starts_with("/cpufreq/")))
                    continue;
                const auto index = parse_uint(suffix.substr(0, separator));
                if(!index || *index > std::numeric_limits<std::uint32_t>::max())
                    continue;
                auto &policy = policies[static_cast<std::uint32_t>(*index)];
                policy.index = static_cast<std::uint32_t>(*index);
                read_policy_field(policy, *record, warnings);
            }
        }
        std::set<std::vector<std::uint32_t>> legacy_members;
        for(auto &[index, policy] : policies)
        {
            (void)index;
            if(!modern_policies && !policy.related_cpus.empty())
            {
                std::vector<std::uint32_t> members;
                members.reserve(policy.related_cpus.size());
                for(auto id : policy.related_cpus)
                    members.push_back(id.value());
                if(!legacy_members.insert(std::move(members)).second)
                    continue;
            }
            cpu.frequency_policies.push_back(std::move(policy));
        }
        for(auto &logical : cpu.logical_cpus)
        {
            if(const auto found = reported_online.find(logical.id.value()); found != reported_online.end())
                logical.online = found->second;
        }
        if(cpu.online_cpu_ids)
        {
            std::set<std::uint32_t> online;
            for(auto id : *cpu.online_cpu_ids)
                online.insert(id.value());
            for(auto &logical : cpu.logical_cpus)
                logical.online = online.contains(logical.id.value());
        }
    }
} // namespace sysal::detail
