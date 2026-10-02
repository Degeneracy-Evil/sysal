#include "parser/cgroup.hpp"
#include "parser/parse_utils.hpp"

#include <filesystem>
#include <limits>
#include <unordered_map>

namespace sysal::detail
{
    namespace
    {
        void adopt_cpu(Cgroup &group, const std::string &quota_text, const std::string &period_text,
                       std::vector<std::string> &warnings)
        {
            const auto period = parse_uint(period_text);
            const auto quota = parse_uint(quota_text);
            const bool unlimited = quota_text == "max" || quota_text == "-1";
            if(!period || *period == 0 || (!unlimited && (!quota || *quota == 0)))
            {
                warnings.push_back("parse_execution: invalid cgroup CPU quota/period");
                return;
            }
            group.cpu_limit_known = true;
            if(!group.cpu_period_us)
                group.cpu_period_us = Microseconds{*period};
            if(unlimited)
                return;
            const auto value = quota.value();
            const auto cycle = period.value();
            if(!group.cpu_quota_us || !group.cpu_period_us ||
               static_cast<long double>(value) / cycle <
                   static_cast<long double>(group.cpu_quota_us->value) / group.cpu_period_us->value)
            {
                group.cpu_quota_us = Microseconds{value};
                group.cpu_period_us = Microseconds{cycle};
            }
        }
    } // namespace

    void parse_cgroup_limits(const RawStore &raw, ExecutionContext &context, std::vector<std::string> &warnings)
    {
        std::unordered_map<std::string, std::string> values;
        for(const auto *record : raw.get_all(RawSource::CgroupFile))
        {
            if(record->status == CollectStatus::Success)
                values.emplace(record->path_or_command, trim(record->payload));
        }
        for(const auto *record : raw.get_all(RawSource::CgroupFile))
        {
            if(record->status != CollectStatus::Success)
                continue;
            const std::filesystem::path path(record->path_or_command);
            const auto name = path.filename().string();
            const auto value = trim(record->payload);
            auto &group = context.cgroup;
            if(name == "cpu.max")
            {
                const auto parts = split(value, ' ');
                std::vector<std::string> tokens;
                for(const auto &part : parts)
                    if(!part.empty())
                        tokens.push_back(part);
                if(tokens.size() == 2)
                    adopt_cpu(group, tokens[0], tokens[1], warnings);
                else
                    warnings.push_back("parse_execution: invalid cpu.max");
            }
            else if(name == "cpu.cfs_quota_us")
            {
                const auto period = values.find((path.parent_path() / "cpu.cfs_period_us").string());
                if(period != values.end())
                    adopt_cpu(group, value, period->second, warnings);
            }
            else if(name == "memory.max" || name == "memory.limit_in_bytes")
            {
                const auto limit = parse_uint(value);
                const bool unlimited =
                    value == "max" ||
                    (name == "memory.limit_in_bytes" && limit &&
                     *limit >= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) - 65535);
                if(unlimited)
                    group.memory_limit_known = true;
                else if(limit)
                {
                    group.memory_limit_known = true;
                    if(!group.memory_limit || *limit < group.memory_limit->value)
                        group.memory_limit = MemorySize{*limit};
                }
                else
                    warnings.push_back("parse_execution: invalid cgroup memory limit");
            }
            else if(name == "memory.current" || name == "memory.usage_in_bytes")
            {
                if(const auto current = parse_uint(value); current && !group.memory_current)
                    group.memory_current = MemorySize{*current};
            }
            else if(name == "cpuset.cpus.effective" && context.cpuset.cpus_effective.empty())
                context.cpuset.cpus_effective = value;
            else if(name == "cpuset.mems.effective" && context.cpuset.mems_effective.empty())
                context.cpuset.mems_effective = value;
        }
    }
} // namespace sysal::detail
