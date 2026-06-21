#include "execution_parser.hpp"
#include "parse_utils.hpp"

#include "sysal/diagnostics.hpp"
#include "sysal/ids.hpp"
#include "sysal/raw_store.hpp"

#include "reader/linux/file_utils.hpp"

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <unistd.h>

namespace sysal::detail
{

namespace
{

std::vector<std::uint32_t> parse_id_list(std::string_view list_str)
{
    std::vector<std::uint32_t> result;
    auto parts = split(list_str, ',');
    for(const auto& part : parts)
    {
        auto trimmed = trim(part);
        if(trimmed.empty())
        {
            continue;
        }
        auto dash = trimmed.find('-');
        if(dash == std::string::npos)
        {
            auto val = parse_uint(trimmed);
            if(val)
            {
                result.push_back(static_cast<std::uint32_t>(*val));
            }
        }
        else
        {
            auto lo = parse_uint(trimmed.substr(0, dash));
            auto hi = parse_uint(trimmed.substr(dash + 1));
            if(lo && hi && *lo <= *hi)
            {
                for(auto v = *lo; v <= *hi; ++v)
                {
                    result.push_back(static_cast<std::uint32_t>(v));
                }
            }
        }
    }
    return result;
}

std::optional<std::string> get_status_field(const std::string& status_content,
                                            std::string_view field_name)
{
    auto lines = split(status_content, '\n');
    for(const auto& line : lines)
    {
        auto [key, value] = split_kv(line, ':');
        if(key == field_name)
        {
            return value;
        }
    }
    return std::nullopt;
}

void parse_cgroup(const RawStore& raw, ExecutionContextInfo& info)
{
    auto records = raw.get_all(RawSource::ProcSelfCgroup);
    if(records.empty())
    {
        return;
    }
    const auto* record = records[0];
    if(record->payload.empty())
    {
        return;
    }

    auto lines = split(record->payload, '\n');
    for(const auto& line : lines)
    {
        if(line.empty())
        {
            continue;
        }
        if(line.starts_with("0::"))
        {
            info.cgroup.version = CgroupVersion::V2;
            info.cgroup.path = line.substr(3);
            return;
        }
    }

    for(const auto& line : lines)
    {
        if(line.empty())
        {
            continue;
        }
        auto pos = line.find(':');
        if(pos != std::string::npos)
        {
            info.cgroup.version = CgroupVersion::V1;
            info.cgroup.path = line.substr(pos + 1);
            return;
        }
    }
}

void parse_cpuset_and_capabilities(const RawStore& raw, ExecutionContextInfo& info)
{
    auto records = raw.get_all(RawSource::ProcSelfStatus);
    if(records.empty())
    {
        return;
    }
    const auto* record = records[0];
    if(record->payload.empty())
    {
        return;
    }

    auto cpus_list = get_status_field(record->payload, "Cpus_allowed_list");
    if(cpus_list)
    {
        auto ids = parse_id_list(*cpus_list);
        for(auto id : ids)
        {
            info.cpuset.cpus.push_back(LogicalCpuId(id));
        }
    }

    auto mems_list = get_status_field(record->payload, "Mems_allowed_list");
    if(mems_list)
    {
        auto ids = parse_id_list(*mems_list);
        for(auto id : ids)
        {
            info.cpuset.mems.push_back(NumaNodeId(id));
        }
    }

    auto cap_eff = get_status_field(record->payload, "CapEff");
    if(cap_eff)
    {
        info.permissions.capabilities.push_back(*cap_eff);
    }
}

std::optional<ContainerInfo> detect_container()
{
    if(read_file("/.dockerenv").has_value())
    {
        return ContainerInfo{ContainerKind::Docker, {}};
    }

    auto proc1_cgroup = read_file("/proc/1/cgroup");
    if(proc1_cgroup)
    {
        const auto& content = *proc1_cgroup;
        if(content.find("docker") != std::string::npos)
        {
            return ContainerInfo{ContainerKind::Docker, {}};
        }
        if(content.find("podman") != std::string::npos)
        {
            return ContainerInfo{ContainerKind::Podman, {}};
        }
        if(content.find("lxc") != std::string::npos)
        {
            return ContainerInfo{ContainerKind::Lxc, {}};
        }
        if(content.find("kubepods") != std::string::npos ||
           content.find("kube") != std::string::npos)
        {
            return ContainerInfo{ContainerKind::Kubernetes, {}};
        }
    }

    if(std::getenv("container") != nullptr) // NOLINT(concurrency-mt-unsafe)
    {
        return ContainerInfo{ContainerKind::Podman, {}};
    }
    if(std::getenv("KUBERNETES_SERVICE_HOST") != nullptr) // NOLINT(concurrency-mt-unsafe)
    {
        return ContainerInfo{ContainerKind::Kubernetes, {}};
    }

    return std::nullopt;
}

void parse_environment(ExecutionContextInfo& info)
{
    const std::vector<std::string> var_names = {
        "CUDA_VISIBLE_DEVICES", "HIP_VISIBLE_DEVICES", "ONEAPI_DEVICE_SELECTOR",
        "OMP_NUM_THREADS",      "MLU_VISIBLE_DEVICES",
    };
    for(const auto& name : var_names)
    {
        const char* val = std::getenv(name.c_str()); // NOLINT(concurrency-mt-unsafe)
        if(val != nullptr)
        {
            info.environment.relevant_vars.emplace_back(name, val);
        }
    }
}

} // namespace

ExecutionContextInfo parse_execution_context(const RawStore& raw, Diagnostics& diag)
{
    ExecutionContextInfo info;
    info.process.pid = getpid();
    info.process.uid = static_cast<int>(getuid());
    info.process.gid = static_cast<int>(getgid());
    info.process.euid = static_cast<int>(geteuid());
    info.process.egid = static_cast<int>(getegid());
    info.permissions.is_root = (geteuid() == 0U);

    parse_cgroup(raw, info);
    parse_cpuset_and_capabilities(raw, info);
    info.container = detect_container();
    parse_environment(info);

    if(info.cgroup.path.empty())
    {
        add_warning(diag, "Could not determine cgroup path", RawSource::ProcSelfCgroup);
    }

    return info;
}

} // namespace sysal::detail
