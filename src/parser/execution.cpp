/// @file execution.cpp
/// @brief 执行上下文解析器实现
/// @details 从 /proc/self 系列文件解析进程环境、权限、cgroup 和容器信息。

#include "execution.hpp"

#include "parse_utils.hpp"

#include <string_view>

namespace sysal::detail
{

namespace
{

/// @brief 解析范围格式字符串为整数列表
/// @param s 范围格式字符串，如 "0-3,5,7-9"
/// @return 展开后的整数列表
std::vector<std::uint32_t> parse_range_list(std::string_view s)
{
    std::vector<std::uint32_t> result;
    auto parts = split(s, ',');
    for(const auto& part : parts)
    {
        auto trimmed = trim(part);
        if(trimmed.empty())
        {
            continue;
        }
        auto dash = trimmed.find('-');
        if(dash != std::string::npos)
        {
            auto lo = parse_uint(trimmed.substr(0, dash));
            auto hi = parse_uint(trimmed.substr(dash + 1));
            if(lo.has_value() && hi.has_value())
            {
                for(auto i = *lo; i <= *hi; ++i)
                {
                    result.push_back(static_cast<std::uint32_t>(i));
                }
            }
        }
        else
        {
            auto val = parse_uint(trimmed);
            if(val.has_value())
            {
                result.push_back(static_cast<std::uint32_t>(*val));
            }
        }
    }
    return result;
}

/// @brief 解析 /proc/self/status 内容
/// @param payload /proc/self/status 文件内容
/// @param ctx ExecutionContext（填充 process、cpuset、permission）
/// @param warnings 警告列表
void parse_proc_self_status(std::string_view payload, ExecutionContext& ctx,
                            [[maybe_unused]] std::vector<std::string>& warnings)
{
    // 格式: "Key:\tValue" 每行
    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto [key, value] = parse_kv(line, ':');
        key = trim(key);
        value = trim(value);

        if(key == "Pid")
        {
            auto v = parse_uint(value);
            if(v.has_value())
            {
                ctx.process.pid = static_cast<std::int32_t>(*v);
            }
        }
        else if(key == "PPid")
        {
            auto v = parse_uint(value);
            if(v.has_value())
            {
                ctx.process.ppid = static_cast<std::int32_t>(*v);
            }
        }
        else if(key == "Name")
        {
            ctx.process.comm = value;
        }
        else if(key == "Uid")
        {
            // 格式: "1000\t1000\t1000\t1000" (real, effective, saved, fs)
            auto parts = split(value, '\t');
            if(!parts.empty())
            {
                auto uid_val = parse_uint(trim(parts[0]));
                if(uid_val.has_value())
                {
                    ctx.process.uid = static_cast<std::uint32_t>(*uid_val);
                }
            }
            if(parts.size() > 1)
            {
                auto euid_val = parse_uint(trim(parts[1]));
                if(euid_val.has_value())
                {
                    ctx.permission.euid = static_cast<std::uint32_t>(*euid_val);
                }
            }
        }
        else if(key == "Gid")
        {
            // 格式: "1000\t1000\t1000\t1000" (real, effective, saved, fs)
            auto parts = split(value, '\t');
            if(!parts.empty())
            {
                auto gid_val = parse_uint(trim(parts[0]));
                if(gid_val.has_value())
                {
                    ctx.process.gid = static_cast<std::uint32_t>(*gid_val);
                }
            }
            if(parts.size() > 1)
            {
                auto egid_val = parse_uint(trim(parts[1]));
                if(egid_val.has_value())
                {
                    ctx.permission.egid = static_cast<std::uint32_t>(*egid_val);
                }
            }
        }
        else if(key == "CapEff")
        {
            ctx.permission.capabilities.push_back(value);
        }
        else if(key == "Cpus_allowed_list")
        {
            ctx.cpuset.cpus = value;
            ctx.cpuset.cpus_effective = value;
            auto ids = parse_range_list(value);
            for(auto id : ids)
            {
                ctx.visible_logical_cpu_ids.push_back(LogicalCpuId{id});
            }
        }
        else if(key == "Mems_allowed_list")
        {
            ctx.cpuset.mems = value;
            ctx.cpuset.mems_effective = value;
        }
    }

    ctx.permission.is_root = (ctx.permission.euid == 0);
}

/// @brief 解析 /proc/self/cgroup 内容
/// @param payload /proc/self/cgroup 文件内容
/// @param ctx ExecutionContext（填充 cgroup）
void parse_proc_self_cgroup(std::string_view payload, ExecutionContext& ctx)
{
    // cgroup v2: "0::/user.slice/user-1000.slice/..."
    // cgroup v1: "cpu,cpuacct:/user.slice/..."
    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto trimmed = trim(line);
        if(trimmed.empty())
        {
            continue;
        }
        if(trimmed.starts_with("0::"))
        {
            ctx.cgroup.version = CgroupVersion::V2;
            ctx.cgroup.path = std::string(trimmed.substr(3));
            return;
        }
        // cgroup v1: 格式为 "hierarchy_id:controllers:path"
        auto first_colon = trimmed.find(':');
        if(first_colon != std::string::npos)
        {
            auto second_colon = trimmed.find(':', first_colon + 1);
            if(second_colon != std::string::npos)
            {
                ctx.cgroup.version = CgroupVersion::V1;
                auto controllers_str =
                    trimmed.substr(first_colon + 1, second_colon - first_colon - 1);
                auto cgroup_path = trimmed.substr(second_colon + 1);
                ctx.cgroup.path = std::string(cgroup_path);
                auto controllers = split(controllers_str, ',');
                for(const auto& c : controllers)
                {
                    auto tc = trim(c);
                    if(!tc.empty())
                    {
                        ctx.cgroup.controllers.push_back(std::string(tc));
                    }
                }
                return;
            }
        }
    }
}

/// @brief 解析环境变量记录
/// @param payload 环境变量记录内容（KEY=VALUE 格式，每行一条）
/// @param ctx ExecutionContext（填充 environment 和可见性索引）
void parse_environment(std::string_view payload, ExecutionContext& ctx)
{
    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto trimmed = trim(line);
        if(trimmed.empty())
        {
            continue;
        }
        auto eq = trimmed.find('=');
        if(eq == std::string::npos)
        {
            continue;
        }
        auto key = std::string(trimmed.substr(0, eq));
        auto value = std::string(trimmed.substr(eq + 1));
        ctx.environment.entries.emplace_back(std::move(key), std::move(value));
    }

    // 从环境变量中提取可见性索引
    for(const auto& [key, value] : ctx.environment.entries)
    {
        if(key == "CUDA_VISIBLE_DEVICES")
        {
            auto ids = split(value, ',');
            for(const auto& id_str : ids)
            {
                auto v = parse_uint(trim(id_str));
                if(v.has_value())
                {
                    ctx.visible_accelerator_ids.push_back(
                        AcceleratorId{static_cast<std::uint32_t>(*v)});
                }
            }
        }
    }
}

/// @brief 检测容器类型
/// @param raw 原始证据存储
/// @param ctx ExecutionContext（读取 environment）
/// @return 容器信息（若检测到容器环境）
std::optional<Container> detect_container(const RawStore& raw, const ExecutionContext& ctx)
{
    // 1. /.dockerenv 存在（Success 状态）→ Docker
    if(raw.has_success(RawSource::RootDockerenv))
    {
        return Container{ContainerKind::Docker, "", ""};
    }

    // 2. /proc/1/cgroup 包含容器标识
    auto cgroup_records = raw.get_all(RawSource::ProcOneCgroup);
    for(const auto* rec : cgroup_records)
    {
        const auto& content = rec->payload;
        if(content.find("docker") != std::string::npos)
        {
            return Container{ContainerKind::Docker, "", ""};
        }
        if(content.find("podman") != std::string::npos)
        {
            return Container{ContainerKind::Podman, "", ""};
        }
        if(content.find("lxc") != std::string::npos)
        {
            return Container{ContainerKind::Lxc, "", ""};
        }
        if(content.find("kubepods") != std::string::npos ||
           content.find("kube") != std::string::npos)
        {
            return Container{ContainerKind::Kubernetes, "", ""};
        }
    }

    // 3. 环境变量检测
    for(const auto& [key, value] : ctx.environment.entries)
    {
        if(key == "container")
        {
            // "container=docker" 或 "container=podman" 等
            if(value == "docker")
            {
                return Container{ContainerKind::Docker, "", ""};
            }
            return Container{ContainerKind::Podman, "", ""};
        }
        if(key == "KUBERNETES_SERVICE_HOST")
        {
            return Container{ContainerKind::Kubernetes, "", ""};
        }
    }

    return std::nullopt;
}

} // namespace

std::optional<ExecutionContext> parse_execution(const RawStore& raw,
                                                std::vector<std::string>& warnings)
{
    ExecutionContext ctx;

    bool has_data = false;

    // 解析 /proc/self/status
    auto status_records = raw.get_all(RawSource::ProcSelfStatus);
    if(!status_records.empty())
    {
        parse_proc_self_status(status_records[0]->payload, ctx, warnings);
        has_data = true;
    }

    // 解析 /proc/self/cgroup
    auto cgroup_records = raw.get_all(RawSource::ProcSelfCgroup);
    if(!cgroup_records.empty())
    {
        parse_proc_self_cgroup(cgroup_records[0]->payload, ctx);
        has_data = true;
    }

    // 解析环境变量
    auto env_records = raw.get_all(RawSource::Environment);
    if(!env_records.empty())
    {
        parse_environment(env_records[0]->payload, ctx);
        has_data = true;
    }

    // 容器检测
    ctx.container = detect_container(raw, ctx);

    // 若无任何数据，返回 nullopt
    if(!has_data)
    {
        return std::nullopt;
    }

    return ctx;
}

} // namespace sysal::detail
