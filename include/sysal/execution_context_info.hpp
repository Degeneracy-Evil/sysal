/// @file execution_context_info.hpp
/// @brief 执行上下文信息数据模型
/// @details 描述当前进程的运行环境，包括进程凭证、环境变量、cgroup、cpuset、
///          权限、容器可见性及当前进程可见的逻辑 CPU/加速器/网络接口。

#pragma once

#include "sysal/enums.hpp"
#include "sysal/ids.hpp"
#include "sysal/value_types.hpp"

#include <string>
#include <utility>
#include <vector>

namespace sysal
{

/// @brief 进程凭证信息
struct ProcessInfo
{
    int pid{};  ///< 进程 ID
    int uid{};  ///< 真实用户 ID
    int gid{};  ///< 真实组 ID
    int euid{}; ///< 有效用户 ID
    int egid{}; ///< 有效组 ID
};

/// @brief 环境变量信息
struct EnvironmentInfo
{
    std::vector<std::pair<std::string, std::string>> relevant_vars; ///< 相关环境变量键值对
};

/// @brief cgroup 信息
struct CgroupInfo
{
    CgroupVersion version{}; ///< cgroup 版本
    std::string path;        ///< cgroup 路径
};

/// @brief cpuset 信息
struct CpusetInfo
{
    std::vector<LogicalCpuId> cpus; ///< 绑定的逻辑 CPU 列表
    std::vector<NumaNodeId> mems;   ///< 绑定的内存节点列表
    bool is_restricted{};           ///< 是否受 cpuset 限制
};

/// @brief 权限信息
struct PermissionInfo
{
    bool is_root{};                        ///< 是否以 root 运行
    std::vector<std::string> capabilities; ///< 持有的 capabilities 列表
};

/// @brief 容器信息
struct ContainerInfo
{
    ContainerKind kind{}; ///< 容器类型
    std::string id;       ///< 容器 ID
};

/// @brief 执行上下文信息聚合
/// @details 汇总当前进程的运行环境及其可见的逻辑 CPU、加速器与网络接口。
struct ExecutionContextInfo
{
    ProcessInfo process;                    ///< 进程凭证
    EnvironmentInfo environment;            ///< 环境变量
    CgroupInfo cgroup;                      ///< cgroup 信息
    CpusetInfo cpuset;                      ///< cpuset 信息
    PermissionInfo permissions;             ///< 权限信息
    std::optional<ContainerInfo> container; ///< 容器信息（可能不在容器中）

    std::vector<LogicalCpuId> visible_logical_cpu_ids;          ///< 当前进程可见逻辑 CPU
    std::vector<AcceleratorId> visible_accelerator_ids;         ///< 当前进程可见加速器
    std::vector<InterfaceName> visible_network_interface_names; ///< 可见网络接口名
};

} // namespace sysal
