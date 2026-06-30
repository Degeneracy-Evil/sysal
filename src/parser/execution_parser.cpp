/// @file execution_parser.cpp
/// @brief 执行上下文解析器实现
/// @details 解析当前进程的执行环境：进程身份（PID/UID/GID/EUID/EGID）、
///          root 判定、cgroup 版本与路径、cpuset（CPU 与 NUMA 内存亲和）、
///          有效能力位、容器类型探测，以及计算相关的环境变量。

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

/// @brief 解析形如 "0-3,5,7-9" 的 ID 列表字符串
/// @param list_str 待解析的列表字符串
/// @return 展开后的无符号整型 ID 列表
/// @details 支持单个数字、闭区间（用 '-' 连接）以及逗号分隔的混合形式。
///          无效片段会被静默跳过。典型来源为 /proc/self/status 中的
///          Cpus_allowed_list 与 Mems_allowed_list。
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
            // 单个 ID
            auto val = parse_uint(trimmed);
            if(val)
            {
                result.push_back(static_cast<std::uint32_t>(*val));
            }
        }
        else
        {
            // 区间 "lo-hi"，展开为闭区间内所有整数
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

/// @brief 从 /proc/self/status 内容中按字段名提取值
/// @param status_content /proc/self/status 的文本内容
/// @param field_name 目标字段名，如 "Cpus_allowed_list"
/// @return 字段对应的值（已裁剪）；未找到时返回 std::nullopt
/// @details 按冒号切分键值对，匹配键名后返回裁剪后的值。
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

/// @brief 从 RawStore 解析 cgroup 版本与路径，写入 info
/// @param raw 原始数据存储
/// @param info 待填充的执行上下文信息
/// @details 读取 /proc/self/cgroup：
///          - 优先识别 v2 的 "0::" 前缀行；
///          - 否则取第一个含冒号的行作为 v1 路径。
///          无数据或内容为空时直接返回，不修改 info。
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
    // 优先匹配 cgroup v2（统一层级，以 "0::" 开头）
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

    // 回退到 cgroup v1：取第一个含冒号行的冒号之后部分
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

/// @brief 从 /proc/self/status 解析 cpuset 与有效能力位，写入 info
/// @param raw 原始数据存储
/// @param info 待填充的执行上下文信息
/// @details 提取字段：
///          - Cpus_allowed_list：可用 CPU 列表；
///          - Mems_allowed_list：可用 NUMA 内存节点列表；
///          - CapEff：有效能力位掩码（十六进制字符串）。
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

    // 解析 CPU 亲和列表
    auto cpus_list = get_status_field(record->payload, "Cpus_allowed_list");
    if(cpus_list)
    {
        auto ids = parse_id_list(*cpus_list);
        for(auto id : ids)
        {
            info.cpuset.cpus.push_back(LogicalCpuId(id));
        }
    }

    // 解析 NUMA 内存节点亲和列表
    auto mems_list = get_status_field(record->payload, "Mems_allowed_list");
    if(mems_list)
    {
        auto ids = parse_id_list(*mems_list);
        for(auto id : ids)
        {
            info.cpuset.mems.push_back(NumaNodeId(id));
        }
    }

    // 解析有效能力位掩码
    auto cap_eff = get_status_field(record->payload, "CapEff");
    if(cap_eff)
    {
        info.permissions.capabilities.push_back(*cap_eff);
    }
}

/// @brief 探测当前进程是否运行在容器中及容器类型
/// @return 识别到容器时返回 ContainerInfo；否则返回 std::nullopt
/// @details 检测顺序：
///          1. /.dockerenv 文件存在 -> Docker；
///          2. /proc/1/cgroup 内容包含 docker/podman/lxc/kubepods 标识；
///          3. 环境变量 container -> Podman；
///          4. 环境变量 KUBERNETES_SERVICE_HOST -> Kubernetes。
///          std::getenv 标注 NOLINT 是因为执行上下文采集本身是单线程快照场景。
std::optional<ContainerInfo> detect_container()
{
    // 优先通过 dockerenv 文件判定 Docker
    if(read_file("/.dockerenv").has_value())
    {
        return ContainerInfo{ContainerKind::Docker, {}};
    }

    // 通过 1 号进程的 cgroup 判定容器运行时
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

    // 通过环境变量兜底判定 Podman / Kubernetes
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

/// @brief 采集与计算加速相关的环境变量，写入 info
/// @param info 待填充的执行上下文信息
/// @details 检查一组预定义的环境变量名，存在则记录"变量名 -> 值"映射，
///          覆盖 CUDA、HIP、oneAPI、OpenMP、寒武纪 MLU 等主流加速栈。
void parse_environment(ExecutionContextInfo& info)
{
    // 关注的与设备可见性 / 并行度相关的环境变量
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

/// @brief 从 RawStore 解析当前进程的执行上下文
/// @param raw 原始数据存储
/// @param diag 诊断信息容器，用于记录解析过程中的告警
/// @return 填充后的 ExecutionContextInfo（始终返回有效值）
/// @details 流程：
///          1. 通过 getpid/getuid 等系统调用获取进程身份与 root 判定；
///          2. 解析 cgroup 版本与路径；
///          3. 解析 cpuset（CPU / 内存节点亲和）与有效能力位；
///          4. 探测容器类型；
///          5. 采集计算相关环境变量；
///          6. 若 cgroup 路径为空则记录告警。
ExecutionContextInfo parse_execution_context(const RawStore& raw, Diagnostics& diag)
{
    ExecutionContextInfo info;
    // 进程身份信息：PID 与各类 UID/GID
    info.process.pid = getpid();
    info.process.uid = static_cast<int>(getuid());
    info.process.gid = static_cast<int>(getgid());
    info.process.euid = static_cast<int>(geteuid());
    info.process.egid = static_cast<int>(getegid());
    // 有效 UID 为 0 即视为 root
    info.permissions.is_root = (geteuid() == 0U);

    parse_cgroup(raw, info);
    parse_cpuset_and_capabilities(raw, info);
    info.container = detect_container();
    parse_environment(info);

    // cgroup 路径无法确定时记录告警
    if(info.cgroup.path.empty())
    {
        add_warning(diag, "Could not determine cgroup path", RawSource::ProcSelfCgroup);
    }

    return info;
}

} // namespace sysal::detail
