#include "resolver/resolve.hpp"

#include "sysal/model/cpu.hpp"
#include "sysal/model/execution.hpp"
#include "sysal/types/ids.hpp"

#include "test_macros.hpp"
#include <string>
#include <vector>

using namespace sysal;
using namespace sysal::detail;

int main()
{
    // ---- 测试 1: CPU 可见性计算（cpuset 约束） ----
    {
        ParseResult result;

        // 构造 4 个逻辑 CPU（id 0-3）
        Cpu cpu;
        for(std::uint32_t i = 0; i < 4; ++i)
        {
            LogicalCpu lc;
            lc.id = LogicalCpuId{i};
            lc.core_id = CpuCoreId{i};
            lc.package_id = CpuPackageId{0};
            lc.visible_to_current_process = true;
            cpu.logical_cpus.push_back(lc);
        }
        result.cpu = std::move(cpu);

        // 构造 ExecutionContext：cpuset 仅允许 CPU 0 和 1
        ExecutionContext exec;
        exec.visible_logical_cpu_ids = {LogicalCpuId{0}, LogicalCpuId{1}};
        result.execution = std::move(exec);

        std::vector<std::string> warnings;
        auto info = resolve(std::move(result), warnings);

        // CPU 0 和 1 可见，2 和 3 不可见
        CHECK(info.cpu.logical_cpus[0].visible_to_current_process == true);
        CHECK(info.cpu.logical_cpus[1].visible_to_current_process == true);
        CHECK(info.cpu.logical_cpus[2].visible_to_current_process == false);
        CHECK(info.cpu.logical_cpus[3].visible_to_current_process == false);

        // 无可见性交叉校验警告（一致）
        bool has_visibility_mismatch = false;
        for(const auto &w : warnings)
        {
            if(w.find("[visibility_mismatch]") != std::string::npos)
            {
                has_visibility_mismatch = true;
            }
        }
        CHECK(!has_visibility_mismatch);
    }

    // ---- 测试 2: CPU 可见性——无 cpuset 约束（全部可见） ----
    {
        ParseResult result;

        Cpu cpu;
        for(std::uint32_t i = 0; i < 4; ++i)
        {
            LogicalCpu lc;
            lc.id = LogicalCpuId{i};
            lc.core_id = CpuCoreId{i};
            lc.package_id = CpuPackageId{0};
            lc.visible_to_current_process = false;
            cpu.logical_cpus.push_back(lc);
        }
        result.cpu = std::move(cpu);

        // 空 visible_logical_cpu_ids → 无约束，全部可见
        ExecutionContext exec;
        result.execution = std::move(exec);

        std::vector<std::string> warnings;
        auto info = resolve(std::move(result), warnings);

        for(const auto &lc : info.cpu.logical_cpus)
        {
            CHECK(lc.visible_to_current_process == true);
        }
    }

    // ---- 测试 3: 加速器可见性计算 ----
    {
        ParseResult result;

        Accelerators acc;
        for(std::uint32_t i = 0; i < 4; ++i)
        {
            AcceleratorDevice dev;
            dev.id = AcceleratorId{i};
            dev.visible_to_current_process = true;
            acc.devices.push_back(dev);
        }
        result.accelerators = std::move(acc);

        // CUDA_VISIBLE_DEVICES=0,2 → 仅加速器 0 和 2 可见
        ExecutionContext exec;
        exec.visible_accelerator_ids = {AcceleratorId{0}, AcceleratorId{2}};
        result.execution = std::move(exec);

        std::vector<std::string> warnings;
        auto info = resolve(std::move(result), warnings);

        CHECK(info.accelerators.devices[0].visible_to_current_process == true);
        CHECK(info.accelerators.devices[1].visible_to_current_process == false);
        CHECK(info.accelerators.devices[2].visible_to_current_process == true);
        CHECK(info.accelerators.devices[3].visible_to_current_process == false);
    }

    // ---- 测试 4: 加速器可见性——无约束（全部可见） ----
    {
        ParseResult result;

        Accelerators acc;
        for(std::uint32_t i = 0; i < 3; ++i)
        {
            AcceleratorDevice dev;
            dev.id = AcceleratorId{i};
            dev.visible_to_current_process = false;
            acc.devices.push_back(dev);
        }
        result.accelerators = std::move(acc);

        // 空 visible_accelerator_ids → 全部可见
        ExecutionContext exec;
        result.execution = std::move(exec);

        std::vector<std::string> warnings;
        auto info = resolve(std::move(result), warnings);

        for(const auto &dev : info.accelerators.devices)
        {
            CHECK(dev.visible_to_current_process == true);
        }
    }

    // ---- 测试 5: 网络接口可见性（v0.0.1 全部可见） ----
    {
        ParseResult result;

        Network net;
        for(int i = 0; i < 3; ++i)
        {
            NetworkInterface iface;
            iface.name = InterfaceName{"eth" + std::to_string(i)};
            iface.visible_to_current_process = false;
            net.interfaces.push_back(iface);
        }
        result.network = std::move(net);

        ExecutionContext exec;
        result.execution = std::move(exec);

        std::vector<std::string> warnings;
        auto info = resolve(std::move(result), warnings);

        for(const auto &iface : info.network.interfaces)
        {
            CHECK(iface.visible_to_current_process == true);
        }
    }

    // ---- 测试 6: 缺失域使用默认构造 ----
    {
        ParseResult result;
        // 所有域为 nullopt

        std::vector<std::string> warnings;
        auto info = resolve(std::move(result), warnings);

        // 默认构造的 Cpu 应有空的 logical_cpus
        CHECK(info.cpu.logical_cpus.empty());
        CHECK(info.accelerators.devices.empty());
        CHECK(info.network.interfaces.empty());
    }

    // ---- 测试 7: 可见性交叉校验——幻影 ID 检测 ----
    {
        ParseResult result;

        Cpu cpu;
        for(std::uint32_t i = 0; i < 4; ++i)
        {
            LogicalCpu lc;
            lc.id = LogicalCpuId{i};
            lc.core_id = CpuCoreId{i};
            lc.package_id = CpuPackageId{0};
            lc.visible_to_current_process = true;
            cpu.logical_cpus.push_back(lc);
        }
        result.cpu = std::move(cpu);

        // visible_logical_cpu_ids 包含 CPU 0,1,2,3 (全部存在) + CPU 99 (不存在)
        ExecutionContext exec;
        exec.visible_logical_cpu_ids = {LogicalCpuId{0}, LogicalCpuId{1}, LogicalCpuId{2}, LogicalCpuId{3},
                                        LogicalCpuId{99}};
        result.execution = std::move(exec);

        std::vector<std::string> warnings;
        auto info = resolve(std::move(result), warnings);

        // CPU 0-3 可见（在索引中且存在）
        CHECK(info.cpu.logical_cpus[0].visible_to_current_process == true);
        CHECK(info.cpu.logical_cpus[1].visible_to_current_process == true);
        CHECK(info.cpu.logical_cpus[2].visible_to_current_process == true);
        CHECK(info.cpu.logical_cpus[3].visible_to_current_process == true);

        // 应检测到幻影 ID cpu_99
        bool has_phantom_warning = false;
        bool has_constraint_warning = false;
        for(const auto &w : warnings)
        {
            if(w.find("[visibility_mismatch]") != std::string::npos && w.find("cpu_99") != std::string::npos)
            {
                has_phantom_warning = true;
            }
            if(w.find("[constraint]") != std::string::npos)
            {
                has_constraint_warning = true;
            }
        }
        CHECK(has_phantom_warning);
        // 5 visible (including phantom) vs 4 total — 5 > 4 so no constraint
        CHECK(!has_constraint_warning);
    }

    // ---- 测试 8: cpuset 约束提示 ----
    {
        ParseResult result;

        Cpu cpu;
        for(std::uint32_t i = 0; i < 4; ++i)
        {
            LogicalCpu lc;
            lc.id = LogicalCpuId{i};
            lc.core_id = CpuCoreId{i};
            lc.package_id = CpuPackageId{0};
            lc.visible_to_current_process = true;
            cpu.logical_cpus.push_back(lc);
        }
        result.cpu = std::move(cpu);

        // cpuset 限制为 2/4 CPU
        ExecutionContext exec;
        exec.visible_logical_cpu_ids = {LogicalCpuId{0}, LogicalCpuId{1}};
        result.execution = std::move(exec);

        std::vector<std::string> warnings;
        auto info = resolve(std::move(result), warnings);

        CHECK(info.cpu.logical_cpus[0].visible_to_current_process == true);
        CHECK(info.cpu.logical_cpus[1].visible_to_current_process == true);
        CHECK(info.cpu.logical_cpus[2].visible_to_current_process == false);
        CHECK(info.cpu.logical_cpus[3].visible_to_current_process == false);

        bool has_constraint = false;
        for(const auto &w : warnings)
        {
            if(w.find("[constraint]") != std::string::npos && w.find("4 total") != std::string::npos &&
               w.find("2 visible") != std::string::npos)
            {
                has_constraint = true;
            }
        }
        CHECK(has_constraint);
    }

    // ---- 测试 9: 加速器幻影 ID 检测 ----
    {
        ParseResult result;

        Accelerators acc;
        for(std::uint32_t i = 0; i < 3; ++i)
        {
            AcceleratorDevice dev;
            dev.id = AcceleratorId{i};
            dev.visible_to_current_process = true;
            acc.devices.push_back(dev);
        }
        result.accelerators = std::move(acc);

        // visible_accelerator_ids 包含 0,1,2 (全部存在) + 99 (不存在)
        ExecutionContext exec;
        exec.visible_accelerator_ids = {AcceleratorId{0}, AcceleratorId{1}, AcceleratorId{2}, AcceleratorId{99}};
        result.execution = std::move(exec);

        std::vector<std::string> warnings;
        auto info = resolve(std::move(result), warnings);

        bool has_phantom = false;
        for(const auto &w : warnings)
        {
            if(w.find("[visibility_mismatch]") != std::string::npos && w.find("accelerator_99") != std::string::npos)
            {
                has_phantom = true;
            }
        }
        CHECK(has_phantom);
    }

    TEST_SUMMARY();
}
