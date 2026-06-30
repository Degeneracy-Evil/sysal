/// @file resolver.cpp
/// @brief 解析器实现，从 ParsedFacts 构建 SystemSnapshot
/// @details 负责将各采集器解析得到的 ParsedFacts 各子字段移动到 SystemSnapshot
///          对应位置，并根据执行上下文的 cpuset 重新计算逻辑 CPU、加速器与
///          网络接口对当前进程的可见性，最后挂载可选的原始证据 RawStore。

#include "resolver.hpp"

#include "parser/parsed_facts.hpp"

#include "sysal/collect_spec.hpp"
#include "sysal/ids.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/system_snapshot.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

namespace sysal::detail
{

namespace
{

/// @brief 填充快照元数据
/// @details 记录采集起始时刻、sysal 版本、采集耗时，并回填用户请求的 CollectSpec。
///          同时根据 ParsedFacts 中各子字段是否存在，列出成功完成的采集器名称。
/// @param snapshot 待填充元数据的快照
/// @param spec 本次采集规格
/// @param start_time 采集起始时刻
/// @param facts 已解析的中间事实
void fill_meta(SystemSnapshot& snapshot, const CollectSpec& spec,
               std::chrono::system_clock::time_point start_time, const ParsedFacts& facts)
{
    snapshot.meta.collect_time = start_time;
    snapshot.meta.sysal_version = "0.0.1";
    snapshot.meta.collect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start_time);
    snapshot.meta.requested_spec = spec;

    // 依据各字段是否成功解析，登记成功采集的采集器名称
    if(facts.platform)
    {
        snapshot.meta.succeeded_collectors.emplace_back("platform");
    }
    if(facts.cpu)
    {
        snapshot.meta.succeeded_collectors.emplace_back("cpu");
    }
    if(facts.memory)
    {
        snapshot.meta.succeeded_collectors.emplace_back("memory");
    }
    if(facts.network)
    {
        snapshot.meta.succeeded_collectors.emplace_back("network");
    }
    if(facts.pci)
    {
        snapshot.meta.succeeded_collectors.emplace_back("pci");
    }
    if(facts.accelerators)
    {
        snapshot.meta.succeeded_collectors.emplace_back("accelerators");
    }
    if(facts.storage)
    {
        snapshot.meta.succeeded_collectors.emplace_back("storage");
    }
    if(facts.topology)
    {
        snapshot.meta.succeeded_collectors.emplace_back("topology");
    }
    if(facts.software)
    {
        snapshot.meta.succeeded_collectors.emplace_back("software");
    }
    if(facts.execution)
    {
        snapshot.meta.succeeded_collectors.emplace_back("execution");
    }
}

} // namespace

/// @brief 从已解析事实构建最终的 SystemSnapshot
/// @details 首先填充元数据，随后逐项将 ParsedFacts 中存在的子字段移动到快照
///          对应位置；移动完成后对 CPU、网络、加速器默认标记为对当前进程可见，
///          再用执行上下文中的 cpuset 覆盖逻辑 CPU 的可见性并计算是否受限。
///          最后汇总可见的逻辑 CPU、加速器、网络接口列表并挂载原始证据。
/// @param facts 已解析的中间事实，子字段按需被移动
/// @param spec 本次采集规格，用于回填元数据
/// @param start_time 采集起始时刻，用于计算采集耗时
/// @param raw 可选的原始证据存储，直接挂载到快照
/// @return 构建完成的 SystemSnapshot
SystemSnapshot resolve(ParsedFacts&& facts, const CollectSpec& spec,
                       std::chrono::system_clock::time_point start_time,
                       const std::optional<RawStore>& raw)
{
    SystemSnapshot snapshot;

    fill_meta(snapshot, spec, start_time, facts);

    // 将已解析的各子字段移动到快照，缺失项保持默认值
    if(facts.platform)
    {
        snapshot.platform = std::move(*facts.platform);
    }
    if(facts.cpu)
    {
        snapshot.resources.cpu = std::move(*facts.cpu);
        // 默认认为采集到的全部逻辑 CPU 对当前进程可见
        for(auto& cpu : snapshot.resources.cpu.logical_cpus)
        {
            cpu.visible_to_current_process = true;
        }
    }
    if(facts.memory)
    {
        snapshot.resources.memory = std::move(*facts.memory);
    }
    if(facts.network)
    {
        snapshot.resources.network = std::move(*facts.network);
        // 默认认为采集到的全部网络接口对当前进程可见
        for(auto& iface : snapshot.resources.network.interfaces)
        {
            iface.visible_to_current_process = true;
        }
    }
    if(facts.pci)
    {
        snapshot.resources.pci = std::move(*facts.pci);
    }
    if(facts.accelerators)
    {
        snapshot.resources.accelerators = std::move(*facts.accelerators);
        // 默认认为采集到的全部加速器对当前进程可见
        for(auto& dev : snapshot.resources.accelerators.devices)
        {
            dev.visible_to_current_process = true;
        }
    }
    if(facts.storage)
    {
        snapshot.resources.storage = std::move(*facts.storage);
    }
    if(facts.topology)
    {
        snapshot.resources.topology = std::move(*facts.topology);
    }
    if(facts.software)
    {
        snapshot.software = std::move(*facts.software);
    }
    if(facts.execution)
    {
        snapshot.execution = std::move(*facts.execution);
    }

    // 使用集合查找应用 cpuset 可见性（O(n+m) 而非 O(n*m)）
    if(!snapshot.execution.cpuset.cpus.empty() && !snapshot.resources.cpu.logical_cpus.empty())
    {
        std::unordered_set<LogicalCpuId> visible_ids(snapshot.execution.cpuset.cpus.begin(),
                                                     snapshot.execution.cpuset.cpus.end());

        for(auto& cpu : snapshot.resources.cpu.logical_cpus)
        {
            cpu.visible_to_current_process = visible_ids.contains(cpu.id);
        }

        // 仅当 cpuset 是全部 CPU 的真子集时才视为受限
        snapshot.execution.cpuset.is_restricted =
            visible_ids.size() < snapshot.resources.cpu.logical_cpus.size();
    }

    // 汇总当前进程可见的逻辑 CPU 列表
    snapshot.execution.visible_logical_cpu_ids.clear();
    for(const auto& cpu : snapshot.resources.cpu.logical_cpus)
    {
        if(cpu.visible_to_current_process)
        {
            snapshot.execution.visible_logical_cpu_ids.push_back(cpu.id);
        }
    }

    // 汇总当前进程可见的加速器列表
    snapshot.execution.visible_accelerator_ids.clear();
    for(const auto& dev : snapshot.resources.accelerators.devices)
    {
        if(dev.visible_to_current_process)
        {
            snapshot.execution.visible_accelerator_ids.push_back(dev.id);
        }
    }

    // 汇总当前进程可见的网络接口名列表
    snapshot.execution.visible_network_interface_names.clear();
    for(const auto& iface : snapshot.resources.network.interfaces)
    {
        if(iface.visible_to_current_process)
        {
            snapshot.execution.visible_network_interface_names.push_back(iface.name);
        }
    }

    snapshot.raw = raw;

    return snapshot;
}

} // namespace sysal::detail
