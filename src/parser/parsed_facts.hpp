/// @file parsed_facts.hpp
/// @brief 解析器输出聚合结构 ParsedFacts 的定义
/// @details ParsedFacts 是所有解析器产出的结构化事实的统一容器，
///          各子系统解析成功则填充对应字段，失败则保留 std::nullopt。
///          该结构由上层 resolver 进一步组装为 SystemSnapshot。

#pragma once

#include "sysal/execution_context_info.hpp"
#include "sysal/platform_info.hpp"
#include "sysal/resource_info.hpp"
#include "sysal/software_stack_info.hpp"

#include <optional>

namespace sysal::detail
{

/// @brief 所有解析器输出的事实聚合
/// @details 每个字段对应一个独立的解析器输出：
///          - 解析成功且产出有效数据时，字段持有对应结构；
///          - 解析失败、无数据或不适用时，字段为 std::nullopt。
///          上层根据 std::optional 是否有值决定是否纳入最终快照。
struct ParsedFacts
{
    std::optional<PlatformInfo> platform; ///< 平台信息：主机、OS、内核、架构、固件、虚拟化等
    std::optional<CpuSubsystem> cpu; ///< CPU 子系统：逻辑核、物理核、Socket、频率等
    std::optional<MemorySubsystem> memory; ///< 内存子系统：总容量、NUMA 节点内存分布等
    std::optional<PciSubsystem> pci;       ///< PCI 子系统：设备列表、地址、厂商等
    std::optional<NetworkSubsystem> network; ///< 网络子系统：网卡列表、链路状态、NUMA 亲和等
    std::optional<AcceleratorSubsystem> accelerators; ///< 加速器子系统：GPU 等加速设备信息
    std::optional<StorageSubsystem> storage; ///< 存储子系统：块设备、容量、类型、PCI 地址等
    std::optional<TopologyInfo> topology; ///< 拓扑信息：NUMA 关系、设备与 NUMA 节点的亲和
    std::optional<SoftwareStackInfo> software; ///< 软件栈：驱动、运行时、编译器、CUDA/ROCm 等
    std::optional<ExecutionContextInfo>
        execution; ///< 执行上下文：进程信息、cgroup、cpuset、容器、环境变量等
};

} // namespace sysal::detail
