/// @file execution.hpp
/// @brief 执行上下文数据模型
/// @details 定义执行上下文的数据结构：ExecutionContext 及其子结构体
///          （Process、Environment、Cgroup、Cpuset、Permission、Container），
///          描述当前进程的限制与环境，以及可见性便利索引。

#pragma once

#include "sysal/types/enums.hpp"
#include "sysal/types/ids.hpp"
#include "sysal/types/units.hpp"
#include "sysal/types/value_types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal
{

    /// @brief 当前进程
    struct Process
    {
        std::int32_t pid{};  ///< 进程 ID
        std::int32_t ppid{}; ///< 父进程 ID
        std::uint32_t uid{}; ///< 用户 ID
        std::uint32_t gid{}; ///< 组 ID
        std::string comm;    ///< 进程命令名
        std::string exe;     ///< 可执行文件路径
        std::string cwd;     ///< 当前工作目录
    };

    /// @brief 环境变量
    struct Environment
    {
        std::vector<std::pair<std::string, std::string>> entries; ///< 环境变量键值对
    };

    /// @brief cgroup 约束
    struct Cgroup
    {
        CgroupVersion version{};                     ///< cgroup 版本
        std::string path;                            ///< cgroup 路径
        std::vector<std::string> controllers;        ///< cgroup 控制器列表
        std::optional<Microseconds> cpu_quota_us{};  ///< 最严格祖先的 CPU 时间配额
        std::optional<Microseconds> cpu_period_us{}; ///< 对应 CPU 调度周期
        std::optional<MemorySize> memory_limit{};    ///< 层级有效内存上限（字节）
        std::optional<MemorySize> memory_current{};  ///< 当前 cgroup 内存使用量（字节）
        bool cpu_limit_known{};                      ///< 可读层级 CPU 限制完整；quota 缺失表示无限制
        bool memory_limit_known{};                   ///< 可读层级内存限制完整；limit 缺失表示无限制
    };

    /// @brief cpuset 约束
    struct Cpuset
    {
        std::string cpus;           ///< CPU 集合
        std::string mems;           ///< 内存节点集合
        std::string cpus_effective; ///< 有效 CPU 集合
        std::string mems_effective; ///< 有效内存节点集合
    };

    /// @brief 权限
    struct Permission
    {
        std::uint32_t euid{};                  ///< 有效用户 ID
        std::uint32_t egid{};                  ///< 有效组 ID
        std::vector<std::string> capabilities; ///< 能力列表
        bool is_root{};                        ///< 是否为 root
    };

    /// @brief 容器
    struct Container
    {
        ContainerKind kind{}; ///< 容器类型
        std::string id;       ///< 容器 ID
        std::string runtime;  ///< 容器运行时
    };

    /// @brief 执行上下文
    /// @details 描述当前进程的限制与环境，以及可见性便利索引。
    struct ExecutionContext
    {
        Process process;                    ///< 当前进程
        Environment environment;            ///< 环境变量
        Cgroup cgroup;                      ///< cgroup 约束
        Cpuset cpuset;                      ///< cpuset 约束
        Permission permission;              ///< 权限
        std::optional<Container> container; ///< 容器（若在容器内）

        std::vector<LogicalCpuId> visible_logical_cpu_ids;          ///< 可见逻辑 CPU ID
        std::vector<AcceleratorId> visible_accelerator_ids;         ///< 可见加速器 ID
        std::vector<InterfaceName> visible_network_interface_names; ///< 可见网络接口名
        bool cpu_visibility_known{};              ///< 已完整解析 Cpus_allowed_list，空索引表示无可用 CPU
        bool accelerator_visibility_restricted{}; ///< 有显式加速器选择约束，空索引表示全部隐藏
    };

} // namespace sysal
