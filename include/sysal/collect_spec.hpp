/// @file collect_spec.hpp
/// @brief 采集规格定义
/// @details 通过建造者模式配置需要采集的系统信息子集，作为采集入口的参数。

#pragma once

namespace sysal
{

/// @brief 系统信息采集规格
/// @details 以布尔开关描述各子系统是否采集，提供预设工厂与链式 with_* 方法。
class CollectSpec
{
public:
    /// @brief 基础预设：平台、CPU、内存、执行上下文
    /// @return 基础采集规格
    static CollectSpec basic()
    {
        CollectSpec spec;
        spec.platform_ = true;
        spec.cpu_ = true;
        spec.memory_ = true;
        spec.execution_context_ = true;
        return spec;
    }

    /// @brief 全量预设：采集所有子系统并保留原始数据
    /// @return 全量采集规格
    static CollectSpec full()
    {
        CollectSpec spec;
        spec.raw_ = true;
        spec.platform_ = true;
        spec.cpu_ = true;
        spec.memory_ = true;
        spec.accelerators_ = true;
        spec.network_ = true;
        spec.storage_ = true;
        spec.pci_ = true;
        spec.topology_ = true;
        spec.software_stack_ = true;
        spec.execution_context_ = true;
        return spec;
    }

    /// @brief 算子调度预设：平台、CPU、内存、加速器、网络、拓扑、软件栈、执行上下文
    /// @return 面向算子调度的采集规格
    static CollectSpec for_operator_dispatch()
    {
        CollectSpec spec;
        spec.platform_ = true;
        spec.cpu_ = true;
        spec.memory_ = true;
        spec.accelerators_ = true;
        spec.network_ = true;
        spec.topology_ = true;
        spec.software_stack_ = true;
        spec.execution_context_ = true;
        return spec;
    }

    /// @brief 设置是否保留原始数据
    /// @param enabled 是否启用
    /// @return 当前规格引用（用于链式调用）
    CollectSpec& with_raw(bool enabled = true)
    {
        raw_ = enabled;
        return *this;
    }
    /// @brief 设置是否采集平台信息
    CollectSpec& with_platform(bool enabled = true)
    {
        platform_ = enabled;
        return *this;
    }
    /// @brief 设置是否采集 CPU 信息
    CollectSpec& with_cpu(bool enabled = true)
    {
        cpu_ = enabled;
        return *this;
    }
    /// @brief 设置是否采集内存信息
    CollectSpec& with_memory(bool enabled = true)
    {
        memory_ = enabled;
        return *this;
    }
    /// @brief 设置是否采集加速器信息
    CollectSpec& with_accelerators(bool enabled = true)
    {
        accelerators_ = enabled;
        return *this;
    }
    /// @brief 设置是否采集网络信息
    CollectSpec& with_network(bool enabled = true)
    {
        network_ = enabled;
        return *this;
    }
    /// @brief 设置是否采集存储信息
    CollectSpec& with_storage(bool enabled = true)
    {
        storage_ = enabled;
        return *this;
    }
    /// @brief 设置是否采集 PCI 信息
    CollectSpec& with_pci(bool enabled = true)
    {
        pci_ = enabled;
        return *this;
    }
    /// @brief 设置是否采集拓扑信息
    CollectSpec& with_topology(bool enabled = true)
    {
        topology_ = enabled;
        return *this;
    }
    /// @brief 设置是否采集软件栈信息
    CollectSpec& with_software_stack(bool enabled = true)
    {
        software_stack_ = enabled;
        return *this;
    }
    /// @brief 设置是否采集执行上下文信息
    CollectSpec& with_execution_context(bool enabled = true)
    {
        execution_context_ = enabled;
        return *this;
    }

    /// @brief 是否保留原始数据
    bool keep_raw() const { return raw_; }
    /// @brief 是否采集平台信息
    bool collect_platform() const { return platform_; }
    /// @brief 是否采集 CPU 信息
    bool collect_cpu() const { return cpu_; }
    /// @brief 是否采集内存信息
    bool collect_memory() const { return memory_; }
    /// @brief 是否采集加速器信息
    bool collect_accelerators() const { return accelerators_; }
    /// @brief 是否采集网络信息
    bool collect_network() const { return network_; }
    /// @brief 是否采集存储信息
    bool collect_storage() const { return storage_; }
    /// @brief 是否采集 PCI 信息
    bool collect_pci() const { return pci_; }
    /// @brief 是否采集拓扑信息
    bool collect_topology() const { return topology_; }
    /// @brief 是否采集软件栈信息
    bool collect_software_stack() const { return software_stack_; }
    /// @brief 是否采集执行上下文信息
    bool collect_execution_context() const { return execution_context_; }

private:
    bool raw_{false};               ///< 是否保留原始数据
    bool platform_{false};          ///< 是否采集平台信息
    bool cpu_{false};               ///< 是否采集 CPU 信息
    bool memory_{false};            ///< 是否采集内存信息
    bool accelerators_{false};      ///< 是否采集加速器信息
    bool network_{false};           ///< 是否采集网络信息
    bool storage_{false};           ///< 是否采集存储信息
    bool pci_{false};               ///< 是否采集 PCI 信息
    bool topology_{false};          ///< 是否采集拓扑信息
    bool software_stack_{false};    ///< 是否采集软件栈信息
    bool execution_context_{false}; ///< 是否采集执行上下文信息
};

} // namespace sysal
