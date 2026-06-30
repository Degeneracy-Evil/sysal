/// @file resolve.cpp
/// @brief 冲突解决与可见性计算实现
/// @details 将 ParseResult 合并为 SystemInfo：移动各域字段、计算可见性、
///          交叉校验便利索引与资源级 visible_to_current_process 的一致性。

#include "resolver/resolve.hpp"

#include <algorithm>
#include <unordered_set>

namespace sysal::detail
{

namespace
{

/// @brief 计算逻辑 CPU 可见性
/// @details 根据 ExecutionContext.cpuset 中的可见 CPU ID 列表，
///          设置每个 LogicalCpu 的 visible_to_current_process。
///          若 cpuset 为空（无约束），所有 CPU 均可见。
void compute_cpu_visibility(Cpu& cpu, const ExecutionContext& exec)
{
    if(exec.visible_logical_cpu_ids.empty())
    {
        for(auto& lc : cpu.logical_cpus)
        {
            lc.visible_to_current_process = true;
        }
        return;
    }

    std::unordered_set<std::uint32_t> visible_set;
    visible_set.reserve(exec.visible_logical_cpu_ids.size());
    for(const auto& id : exec.visible_logical_cpu_ids)
    {
        visible_set.insert(id.value());
    }

    for(auto& lc : cpu.logical_cpus)
    {
        lc.visible_to_current_process = visible_set.count(lc.id.value()) != 0;
    }
}

/// @brief 计算加速器可见性
/// @details 根据 ExecutionContext.visible_accelerator_ids 设置每个
///          AcceleratorDevice 的 visible_to_current_process。
///          若列表为空（无 CUDA_VISIBLE_DEVICES 等约束），所有加速器均可见。
void compute_accelerator_visibility(Accelerators& acc, const ExecutionContext& exec)
{
    if(exec.visible_accelerator_ids.empty())
    {
        for(auto& dev : acc.devices)
        {
            dev.visible_to_current_process = true;
        }
        return;
    }

    std::unordered_set<std::uint32_t> visible_set;
    visible_set.reserve(exec.visible_accelerator_ids.size());
    for(const auto& id : exec.visible_accelerator_ids)
    {
        visible_set.insert(id.value());
    }

    for(auto& dev : acc.devices)
    {
        dev.visible_to_current_process = visible_set.count(dev.id.value()) != 0;
    }
}

/// @brief 计算网络接口可见性
/// @details v0.0.1 中网络命名空间检测推迟，所有接口均可见。
void compute_network_visibility(Network& net)
{
    for(auto& iface : net.interfaces)
    {
        iface.visible_to_current_process = true;
    }
}

/// @brief 交叉校验 CPU 可见性一致性
/// @details 比较各 LogicalCpu 的 visible_to_current_process 与
///          ExecutionContext.visible_logical_cpu_ids 便利索引。
///          不一致时追加警告。
void cross_check_cpu_visibility(const Cpu& cpu, const ExecutionContext& exec,
                                std::vector<std::string>& warnings)
{
    if(exec.visible_logical_cpu_ids.empty())
    {
        return;
    }

    std::unordered_set<std::uint32_t> index_set;
    for(const auto& id : exec.visible_logical_cpu_ids)
    {
        index_set.insert(id.value());
    }

    for(const auto& lc : cpu.logical_cpus)
    {
        bool in_index = index_set.count(lc.id.value()) != 0;
        if(lc.visible_to_current_process != in_index)
        {
            warnings.push_back("[visibility_mismatch] cpu_" + std::to_string(lc.id.value()) +
                               ": visible_to_current_process=" +
                               (lc.visible_to_current_process ? "true" : "false") +
                               ", in_visible_logical_cpu_ids=" + (in_index ? "true" : "false"));
        }
    }
}

/// @brief 交叉校验加速器可见性一致性
void cross_check_accelerator_visibility(const Accelerators& acc, const ExecutionContext& exec,
                                        std::vector<std::string>& warnings)
{
    if(exec.visible_accelerator_ids.empty())
    {
        return;
    }

    std::unordered_set<std::uint32_t> index_set;
    for(const auto& id : exec.visible_accelerator_ids)
    {
        index_set.insert(id.value());
    }

    for(const auto& dev : acc.devices)
    {
        bool in_index = index_set.count(dev.id.value()) != 0;
        if(dev.visible_to_current_process != in_index)
        {
            warnings.push_back("[visibility_mismatch] accelerator_" +
                               std::to_string(dev.id.value()) + ": visible_to_current_process=" +
                               (dev.visible_to_current_process ? "true" : "false") +
                               ", in_visible_accelerator_ids=" + (in_index ? "true" : "false"));
        }
    }
}

/// @brief 交叉校验网络接口可见性一致性
/// @details v0.0.1 中网络命名空间检测推迟，便利索引为空，
///          所有接口均标记可见，无交叉校验必要。
void cross_check_network_visibility(const Network& /*net*/, const ExecutionContext& /*exec*/,
                                    std::vector<std::string>& /*warnings*/)
{
    // v0.0.1: 网络命名空间检测推迟，visible_network_interface_names 为空，
    // 所有接口均标记可见，无需交叉校验。
}

} // namespace

SystemInfo resolve(ParseResult result, std::vector<std::string>& warnings)
{
    SystemInfo info;

    // 移动各域字段：若 optional 有值则移动，否则保留默认构造
    info.platform = std::move(result.platform).value_or(Platform{});
    info.cpu = std::move(result.cpu).value_or(Cpu{});
    info.memory = std::move(result.memory).value_or(Memory{});
    info.pci = std::move(result.pci).value_or(Pci{});
    info.network = std::move(result.network).value_or(Network{});
    info.accelerators = std::move(result.accelerators).value_or(Accelerators{});
    info.storage = std::move(result.storage).value_or(Storage{});
    info.software = std::move(result.software).value_or(SoftwareStack{});
    info.execution = std::move(result.execution).value_or(ExecutionContext{});

    // 计算可见性：以 ExecutionContext 中的便利索引为依据，
    // 设置各资源子域的 visible_to_current_process 字段。
    compute_cpu_visibility(info.cpu, info.execution);
    compute_accelerator_visibility(info.accelerators, info.execution);
    compute_network_visibility(info.network);

    // 交叉校验：资源级 visible_to_current_process 为事实来源，
    // 便利索引为派生。不一致时记录警告。
    cross_check_cpu_visibility(info.cpu, info.execution, warnings);
    cross_check_accelerator_visibility(info.accelerators, info.execution, warnings);
    cross_check_network_visibility(info.network, info.execution, warnings);

    // 冲突解决框架（v0.0.1）：
    // 来源信任优先级：专用后端 > sysfs > procfs > 命令输出 > 推断/默认值
    // v0.0.1 中大多数字段只有一个来源，冲突罕见。
    // 若两个来源提供不同值，选择高信任来源并追加警告：
    //   [conflict] <field>: <src1>=<val>, <src2>=<val>, adopted=<src>
    // 当前阶段无需具体冲突检测逻辑，框架已就绪。

    return info;
}

} // namespace sysal::detail
