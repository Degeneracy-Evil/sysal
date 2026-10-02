#include "parser/cgroup.hpp"
#include "parser/parse_utils.hpp"

#include <filesystem>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace sysal::detail
{
    namespace
    {
        enum class LimitState
        {
            Unknown,
            Unlimited,
            Finite,
            Incomplete
        };

        struct CpuBudget
        {
            Microseconds quota;
            Microseconds period;
            bool operator<(const CpuBudget &other) const
            {
                return static_cast<long double>(quota.value) / period.value <
                       static_cast<long double>(other.quota.value) / other.period.value;
            }
        };

        template <typename Value> class LimitSummary
        {
        public:
            void adopt(Value value)
            {
                if(!value_ || value < *value_)
                    value_ = value;
                if(state_ != LimitState::Incomplete)
                    state_ = LimitState::Finite;
            }
            void unlimited()
            {
                if(state_ == LimitState::Unknown)
                    state_ = LimitState::Unlimited;
            }
            void unreadable()
            {
                state_ = LimitState::Incomplete;
            }
            [[nodiscard]] bool known() const
            {
                return state_ == LimitState::Unlimited || state_ == LimitState::Finite;
            }
            [[nodiscard]] bool incomplete() const
            {
                return state_ == LimitState::Incomplete;
            }
            const std::optional<Value> &value() const
            {
                return value_;
            }

        private:
            LimitState state_{LimitState::Unknown};
            std::optional<Value> value_;
        };

        struct MemoryLimit
        {
            MemorySize size;
            bool operator<(const MemoryLimit &other) const
            {
                return size.value < other.size.value;
            }
        };

        void read_cpu_limit(LimitSummary<CpuBudget> &limits, std::optional<Microseconds> &unlimited_period,
                            std::string_view quota_text, std::string_view period_text)
        {
            const auto period = parse_uint(period_text);
            const auto quota = parse_uint(quota_text);
            const bool unlimited = quota_text == "max" || quota_text == "-1";
            if(!period || *period == 0 || (!unlimited && (!quota || *quota == 0)))
            {
                limits.unreadable();
                return;
            }
            if(unlimited)
            {
                limits.unlimited();
                if(!unlimited_period)
                    unlimited_period = Microseconds{*period};
            }
            else
                limits.adopt({Microseconds{*quota}, Microseconds{*period}});
        }

        void read_memory_limit(LimitSummary<MemoryLimit> &limits, const std::string &name, std::string_view value)
        {
            const auto limit = parse_uint(value);
            const bool unlimited =
                value == "max" ||
                (name == "memory.limit_in_bytes" && limit &&
                 *limit >= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) - 65535);
            if(unlimited)
                limits.unlimited();
            else if(limit)
                limits.adopt({MemorySize{*limit}});
            else
                limits.unreadable();
        }
    } // namespace

    void parse_cgroup_limits(const RawStore &raw, ExecutionContext &context, std::vector<std::string> &warnings)
    {
        const auto records = raw.get_all(RawSource::CgroupFile);
        std::unordered_map<std::string, std::string> values;
        for(const auto *record : records)
            if(record->status == CollectStatus::Success)
                values.emplace(record->path_or_command, trim(record->payload));
        LimitSummary<CpuBudget> cpu;
        LimitSummary<MemoryLimit> memory;
        std::optional<Microseconds> unlimited_period;
        for(const auto *record : records)
        {
            if(record->status == CollectStatus::NotCollected)
                continue;
            const std::filesystem::path path(record->path_or_command);
            const auto name = path.filename().string();
            const auto value = trim(record->payload);
            const bool readable = record->status == CollectStatus::Success;
            if(name == "cpu.max")
            {
                std::istringstream input(value);
                std::string quota, period, extra;
                if(readable && (input >> quota >> period) && !(input >> extra))
                    read_cpu_limit(cpu, unlimited_period, quota, period);
                else
                    cpu.unreadable();
            }
            else if(name == "cpu.cfs_quota_us")
            {
                const auto period = values.find((path.parent_path() / "cpu.cfs_period_us").string());
                if(readable && period != values.end())
                    read_cpu_limit(cpu, unlimited_period, value, period->second);
                else
                    cpu.unreadable();
            }
            else if(name == "memory.max" || name == "memory.limit_in_bytes")
            {
                if(readable)
                    read_memory_limit(memory, name, value);
                else
                    memory.unreadable();
            }
            else if(readable && (name == "memory.current" || name == "memory.usage_in_bytes"))
            {
                if(const auto current = parse_uint(value); current && !context.cgroup.memory_current)
                    context.cgroup.memory_current = MemorySize{*current};
            }
            else if(readable && name == "cpuset.cpus.effective" && context.cpuset.cpus_effective.empty())
                context.cpuset.cpus_effective = value;
            else if(readable && name == "cpuset.mems.effective" && context.cpuset.mems_effective.empty())
                context.cpuset.mems_effective = value;
        }
        auto &group = context.cgroup;
        group.cpu_limit_known = cpu.known();
        group.memory_limit_known = memory.known();
        if(cpu.value())
        {
            group.cpu_quota_us = cpu.value()->quota;
            group.cpu_period_us = cpu.value()->period;
        }
        else
            group.cpu_period_us = unlimited_period;
        if(memory.value())
            group.memory_limit = memory.value()->size;
        if(cpu.incomplete())
            warnings.push_back("[cgroup] CPU limit hierarchy incomplete; observed quota is only an upper bound");
        if(memory.incomplete())
            warnings.push_back("[cgroup] memory limit hierarchy incomplete; observed limit is only an upper bound");
    }
} // namespace sysal::detail
