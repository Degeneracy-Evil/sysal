#pragma once

#include "sysal/enums.hpp"
#include "sysal/ids.hpp"
#include "sysal/value_types.hpp"

#include <string>
#include <utility>
#include <vector>

namespace sysal
{

struct ProcessInfo
{
    int pid{};
    int uid{};
    int gid{};
    int euid{};
    int egid{};
};

struct EnvironmentInfo
{
    std::vector<std::pair<std::string, std::string>> relevant_vars;
};

struct CgroupInfo
{
    CgroupVersion version{};
    std::string path;
};

struct CpusetInfo
{
    std::vector<LogicalCpuId> cpus;
    std::vector<NumaNodeId> mems;
    bool is_restricted{};
};

struct PermissionInfo
{
    bool is_root{};
    std::vector<std::string> capabilities;
};

struct ContainerInfo
{
    ContainerKind kind{};
    std::string id;
};

struct ExecutionContextInfo
{
    ProcessInfo process;
    EnvironmentInfo environment;
    CgroupInfo cgroup;
    CpusetInfo cpuset;
    PermissionInfo permissions;
    std::optional<ContainerInfo> container;

    std::vector<LogicalCpuId> visible_logical_cpu_ids;
    std::vector<AcceleratorId> visible_accelerator_ids;
    std::vector<InterfaceName> visible_network_interface_names;
};

} // namespace sysal
