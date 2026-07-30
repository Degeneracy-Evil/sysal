/// @file resolve.cpp
/// @brief 冲突解决与可见性计算实现
/// @details 将 ParseResult 合并为 SystemInfo：移动各域字段、计算可见性、
///          交叉校验便利索引与资源级 visible_to_current_process 的一致性。

#include "resolver/resolve.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_set>

namespace sysal::detail
{

    namespace
    {

        /// @brief 计算逻辑 CPU 可见性
        /// @details 根据 ExecutionContext.cpuset 中的可见 CPU ID 列表，
        ///          设置每个 LogicalCpu 的 visible_to_current_process。
        ///          若 cpuset 为空（无约束），所有 CPU 均可见。
        void compute_cpu_visibility(Cpu &cpu, const ExecutionContext &exec)
        {
            if(exec.visible_logical_cpu_ids.empty())
            {
                for(auto &lc : cpu.logical_cpus)
                {
                    lc.visible_to_current_process = true;
                }
                return;
            }

            std::unordered_set<std::uint32_t> visible_set;
            visible_set.reserve(exec.visible_logical_cpu_ids.size());
            for(const auto &id : exec.visible_logical_cpu_ids)
            {
                visible_set.insert(id.value());
            }

            for(auto &lc : cpu.logical_cpus)
            {
                lc.visible_to_current_process = visible_set.count(lc.id.value()) != 0;
            }
        }

        /// @brief 计算加速器可见性
        /// @details 根据 ExecutionContext.visible_accelerator_ids 设置每个
        ///          AcceleratorDevice 的 visible_to_current_process。
        ///          若列表为空（无 CUDA_VISIBLE_DEVICES 等约束），所有加速器均可见。
        void compute_accelerator_visibility(Accelerators &acc, const ExecutionContext &exec)
        {
            if(exec.visible_accelerator_ids.empty())
            {
                for(auto &dev : acc.devices)
                {
                    dev.visible_to_current_process = true;
                }
                return;
            }

            std::unordered_set<std::uint32_t> visible_set;
            visible_set.reserve(exec.visible_accelerator_ids.size());
            for(const auto &id : exec.visible_accelerator_ids)
            {
                visible_set.insert(id.value());
            }

            for(auto &dev : acc.devices)
            {
                dev.visible_to_current_process = visible_set.count(dev.id.value()) != 0;
            }
        }

        /// @brief 计算网络接口可见性
        /// @details v0.0.1 中网络命名空间检测推迟，所有接口均可见。
        void compute_network_visibility(Network &net)
        {
            for(auto &iface : net.interfaces)
            {
                iface.visible_to_current_process = true;
            }
        }

        /// @brief 交叉校验 CPU 可见性
        /// @details 检测两类问题：
        ///          1. 幻影 ID：visible_logical_cpu_ids 引用了模型中不存在的 CPU
        ///          2. 约束提示：cpuset 限制了可见 CPU 数量（信息性，非错误）
        void cross_check_cpu_visibility(const Cpu &cpu, const ExecutionContext &exec,
                                        std::vector<std::string> &warnings)
        {
            if(exec.visible_logical_cpu_ids.empty())
            {
                return;
            }

            // 构建模型中存在的 CPU ID 集合
            std::unordered_set<std::uint32_t> model_ids;
            for(const auto &lc : cpu.logical_cpus)
            {
                model_ids.insert(lc.id.value());
            }

            // 检测幻影 ID：索引引用了模型中不存在的 CPU
            for(const auto &id : exec.visible_logical_cpu_ids)
            {
                if(model_ids.count(id.value()) == 0)
                {
                    warnings.push_back("[visibility_mismatch] cpu_" + std::to_string(id.value()) +
                                       ": in_visible_logical_cpu_ids but cpu does not exist in model");
                }
            }

            // 约束提示：cpuset 限制了可见 CPU 数量
            if(!exec.visible_logical_cpu_ids.empty() && exec.visible_logical_cpu_ids.size() < cpu.logical_cpus.size())
            {
                warnings.push_back(
                    "[constraint] cpu visibility restricted: " + std::to_string(cpu.logical_cpus.size()) + " total, " +
                    std::to_string(exec.visible_logical_cpu_ids.size()) + " visible");
            }
        }

        /// @brief 交叉校验加速器可见性
        /// @details 检测两类问题：
        ///          1. 幻影 ID：visible_accelerator_ids 引用了模型中不存在的加速器
        ///          2. 约束提示：可见加速器数量受环境变量限制（信息性，非错误）
        void cross_check_accelerator_visibility(const Accelerators &acc, const ExecutionContext &exec,
                                                std::vector<std::string> &warnings)
        {
            if(exec.visible_accelerator_ids.empty())
            {
                return;
            }

            // 构建模型中存在的加速器 ID 集合
            std::unordered_set<std::uint32_t> model_ids;
            for(const auto &dev : acc.devices)
            {
                model_ids.insert(dev.id.value());
            }

            // 检测幻影 ID：索引引用了模型中不存在的加速器
            for(const auto &id : exec.visible_accelerator_ids)
            {
                if(model_ids.count(id.value()) == 0)
                {
                    warnings.push_back("[visibility_mismatch] accelerator_" + std::to_string(id.value()) +
                                       ": in_visible_accelerator_ids but accelerator does not exist in model");
                }
            }

            // 约束提示：环境变量限制了可见加速器数量
            if(!exec.visible_accelerator_ids.empty() && exec.visible_accelerator_ids.size() < acc.devices.size())
            {
                warnings.push_back(
                    "[constraint] accelerator visibility restricted: " + std::to_string(acc.devices.size()) +
                    " total, " + std::to_string(exec.visible_accelerator_ids.size()) + " visible");
            }
        }

        /// @brief 交叉校验网络接口可见性一致性
        /// @details v0.0.1 中网络命名空间检测推迟，便利索引为空，
        ///          所有接口均标记可见，无交叉校验必要。
        void cross_check_network_visibility(const Network & /*net*/, const ExecutionContext & /*exec*/,
                                            std::vector<std::string> & /*warnings*/)
        {
            // v0.0.1: 网络命名空间检测推迟，visible_network_interface_names 为空，
            // 所有接口均标记可见，无需交叉校验。
        }

    } // namespace

    SystemInfo resolve(ParseResult result, std::vector<std::string> &warnings)
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

        // 交叉校验：检测幻影 ID 和约束提示
        cross_check_cpu_visibility(info.cpu, info.execution, warnings);
        cross_check_accelerator_visibility(info.accelerators, info.execution, warnings);
        cross_check_network_visibility(info.network, info.execution, warnings);

        return info;
    }

} // namespace sysal::detail
