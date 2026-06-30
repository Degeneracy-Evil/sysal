#include "parser/cpu.hpp"

#include "sysal/model/raw_store.hpp"
#include "sysal/types/enums.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

using namespace sysal;
using namespace sysal::detail;

/// @brief 创建一条 RawRecord
static RawRecord make_record(RawSource source, const std::string& path, const std::string& payload)
{
    return RawRecord{source, path, payload, CollectStatus::Success,
                     std::chrono::system_clock::now()};
}

int main()
{
    // ---- 测试 1: 4 逻辑 CPU，2 封装，每封装 2 核 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcCpuInfo, "/proc/cpuinfo",
                                          "processor\t: 0\n"
                                          "vendor_id\t: GenuineIntel\n"
                                          "model name\t: Intel(R) Xeon(R) Gold 6330\n"
                                          "physical id\t: 0\n"
                                          "core id\t\t: 0\n"
                                          "flags\t\t: fpu sse4_2 avx avx2 avx512f avx512cd\n"
                                          "\n"
                                          "processor\t: 1\n"
                                          "vendor_id\t: GenuineIntel\n"
                                          "model name\t: Intel(R) Xeon(R) Gold 6330\n"
                                          "physical id\t: 0\n"
                                          "core id\t\t: 1\n"
                                          "flags\t\t: fpu sse4_2 avx avx2 avx512f avx512cd\n"
                                          "\n"
                                          "processor\t: 2\n"
                                          "vendor_id\t: GenuineIntel\n"
                                          "model name\t: Intel(R) Xeon(R) Gold 6330\n"
                                          "physical id\t: 1\n"
                                          "core id\t\t: 0\n"
                                          "flags\t\t: fpu sse4_2 avx avx2 avx512f avx512cd\n"
                                          "\n"
                                          "processor\t: 3\n"
                                          "vendor_id\t: GenuineIntel\n"
                                          "model name\t: Intel(R) Xeon(R) Gold 6330\n"
                                          "physical id\t: 1\n"
                                          "core id\t\t: 1\n"
                                          "flags\t\t: fpu sse4_2 avx avx2 avx512f avx512cd\n"));

        std::vector<std::string> warnings;
        auto result = parse_cpu(raw, warnings);
        assert(result.has_value());

        const auto& cpu = *result;

        // 2 封装
        assert(cpu.packages.size() == 2);
        // 4 物理核
        assert(cpu.cores.size() == 4);
        // 4 逻辑 CPU
        assert(cpu.logical_cpus.size() == 4);

        // 封装属性
        assert(cpu.packages[0].physical_cores == 2);
        assert(cpu.packages[0].logical_threads == 2);
        assert(cpu.packages[1].physical_cores == 2);
        assert(cpu.packages[1].logical_threads == 2);

        // 型号名称
        assert(cpu.packages[0].model_name.value == "Intel(R) Xeon(R) Gold 6330");
        assert(cpu.packages[0].vendor.value == "GenuineIntel");

        // ISA 扩展
        assert(cpu.isa_extensions.size() == 5); // sse4_2, avx, avx2, avx512f, avx512cd
        assert(cpu.arch == Arch::X86_64);

        // 逻辑 CPU 可见性
        for(const auto& lc : cpu.logical_cpus)
        {
            assert(lc.visible_to_current_process == true);
        }
    }

    // ---- 测试 2: 缺少 physical_id（默认为 0） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcCpuInfo, "/proc/cpuinfo",
                                          "processor\t: 0\n"
                                          "model name\t: ARM Cortex-A72\n"
                                          "flags\t\t: fp asimd\n"
                                          "\n"
                                          "processor\t: 1\n"
                                          "model name\t: ARM Cortex-A72\n"
                                          "flags\t\t: fp asimd\n"));

        std::vector<std::string> warnings;
        auto result = parse_cpu(raw, warnings);
        assert(result.has_value());

        const auto& cpu = *result;
        // 所有 CPU 归入封装 0
        assert(cpu.packages.size() == 1);
        assert(cpu.packages[0].logical_threads == 2);
    }

    // ---- 测试 3: 缺少 core_id（默认为 processor 编号） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcCpuInfo, "/proc/cpuinfo",
                                          "processor\t: 0\n"
                                          "physical id\t: 0\n"
                                          "model name\t: Test CPU\n"
                                          "flags\t\t: sse4_2\n"
                                          "\n"
                                          "processor\t: 1\n"
                                          "physical id\t: 0\n"
                                          "model name\t: Test CPU\n"
                                          "flags\t\t: sse4_2\n"));

        std::vector<std::string> warnings;
        auto result = parse_cpu(raw, warnings);
        assert(result.has_value());

        const auto& cpu = *result;
        // core_id 默认为 processor 编号，所以 2 个不同的核
        assert(cpu.cores.size() == 2);
        assert(cpu.logical_cpus.size() == 2);
    }

    // ---- 测试 4: NUMA 映射 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcCpuInfo, "/proc/cpuinfo",
                                          "processor\t: 0\n"
                                          "physical id\t: 0\n"
                                          "core id\t\t: 0\n"
                                          "flags\t\t: sse4_2\n"
                                          "\n"
                                          "processor\t: 1\n"
                                          "physical id\t: 0\n"
                                          "core id\t\t: 1\n"
                                          "flags\t\t: sse4_2\n"
                                          "\n"
                                          "processor\t: 2\n"
                                          "physical id\t: 1\n"
                                          "core id\t\t: 0\n"
                                          "flags\t\t: sse4_2\n"
                                          "\n"
                                          "processor\t: 3\n"
                                          "physical id\t: 1\n"
                                          "core id\t\t: 1\n"
                                          "flags\t\t: sse4_2\n"));
        raw.records.push_back(make_record(RawSource::SysfsNuma, "node/node0/cpulist", "0-1\n"));
        raw.records.push_back(make_record(RawSource::SysfsNuma, "node/node1/cpulist", "2-3\n"));

        std::vector<std::string> warnings;
        auto result = parse_cpu(raw, warnings);
        assert(result.has_value());

        const auto& cpu = *result;
        // NUMA 节点
        assert(cpu.numa_nodes.size() == 2);
        assert(cpu.numa_nodes[0].id == NumaNodeId{0});
        assert(cpu.numa_nodes[0].cpus.size() == 2);
        assert(cpu.numa_nodes[1].id == NumaNodeId{1});
        assert(cpu.numa_nodes[1].cpus.size() == 2);

        // LogicalCpu 的 numa_node
        assert(cpu.logical_cpus[0].numa_node.has_value());
        assert(*cpu.logical_cpus[0].numa_node == NumaNodeId{0});
        assert(cpu.logical_cpus[2].numa_node.has_value());
        assert(*cpu.logical_cpus[2].numa_node == NumaNodeId{1});

        // CpuCore 的 numa_node
        assert(cpu.cores[0].numa_node.has_value());
        assert(*cpu.cores[0].numa_node == NumaNodeId{0});
    }

    // ---- 测试 5: cpufreq 频率信息 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcCpuInfo, "/proc/cpuinfo",
                                          "processor\t: 0\n"
                                          "physical id\t: 0\n"
                                          "core id\t\t: 0\n"
                                          "flags\t\t: sse4_2\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsCpu, "cpu/cpu0/cpufreq/base_frequency", "2300000\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsCpu, "cpu/cpu0/cpufreq/scaling_max_freq", "3300000\n"));

        std::vector<std::string> warnings;
        auto result = parse_cpu(raw, warnings);
        assert(result.has_value());

        const auto& cpu = *result;
        assert(cpu.packages[0].base_frequency.has_value());
        // 2300000 kHz → 2300000000 Hz
        assert(cpu.packages[0].base_frequency->value == 2300000ULL * 1000);
        assert(cpu.packages[0].max_frequency.has_value());
        assert(cpu.packages[0].max_frequency->value == 3300000ULL * 1000);
    }

    // ---- 测试 7: 多封装 cpufreq 频率信息（各封装独立频率） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcCpuInfo, "/proc/cpuinfo",
                                          "processor\t: 0\n"
                                          "physical id\t: 0\n"
                                          "core id\t\t: 0\n"
                                          "flags\t\t: sse4_2\n"
                                          "\n"
                                          "processor\t: 1\n"
                                          "physical id\t: 0\n"
                                          "core id\t\t: 1\n"
                                          "flags\t\t: sse4_2\n"
                                          "\n"
                                          "processor\t: 2\n"
                                          "physical id\t: 1\n"
                                          "core id\t\t: 0\n"
                                          "flags\t\t: sse4_2\n"
                                          "\n"
                                          "processor\t: 3\n"
                                          "physical id\t: 1\n"
                                          "core id\t\t: 1\n"
                                          "flags\t\t: sse4_2\n"));
        // Package 0: CPU 0 and 1
        raw.records.push_back(
            make_record(RawSource::SysfsCpu, "cpu/cpu0/cpufreq/base_frequency", "2400000\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsCpu, "cpu/cpu0/cpufreq/scaling_max_freq", "3500000\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsCpu, "cpu/cpu1/cpufreq/base_frequency", "2400000\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsCpu, "cpu/cpu1/cpufreq/scaling_max_freq", "3500000\n"));
        // Package 1: CPU 2 and 3 — different frequencies
        raw.records.push_back(
            make_record(RawSource::SysfsCpu, "cpu/cpu2/cpufreq/base_frequency", "1800000\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsCpu, "cpu/cpu2/cpufreq/scaling_max_freq", "2900000\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsCpu, "cpu/cpu3/cpufreq/base_frequency", "1800000\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsCpu, "cpu/cpu3/cpufreq/scaling_max_freq", "2900000\n"));

        std::vector<std::string> warnings;
        auto result = parse_cpu(raw, warnings);
        assert(result.has_value());

        const auto& cpu = *result;
        assert(cpu.packages.size() == 2);

        // Package 0: 2400000 kHz → 2400000000 Hz
        assert(cpu.packages[0].base_frequency.has_value());
        assert(cpu.packages[0].base_frequency->value == 2400000ULL * 1000);
        assert(cpu.packages[0].max_frequency.has_value());
        assert(cpu.packages[0].max_frequency->value == 3500000ULL * 1000);

        // Package 1: 1800000 kHz → 1800000000 Hz (DIFFERENT from package 0)
        assert(cpu.packages[1].base_frequency.has_value());
        assert(cpu.packages[1].base_frequency->value == 1800000ULL * 1000);
        assert(cpu.packages[1].max_frequency.has_value());
        assert(cpu.packages[1].max_frequency->value == 2900000ULL * 1000);

        // Verify they are actually different
        assert(cpu.packages[0].base_frequency->value != cpu.packages[1].base_frequency->value);
        assert(cpu.packages[0].max_frequency->value != cpu.packages[1].max_frequency->value);
    }

    // ---- 测试 6: 缺少 /proc/cpuinfo 数据 ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_cpu(raw, warnings);
        assert(!result.has_value());
        assert(!warnings.empty());
    }

    return 0;
}
