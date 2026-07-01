#include "parser/cpu.hpp"

#include "sysal/model/raw_store.hpp"
#include "sysal/types/enums.hpp"

#include "test_macros.hpp"
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
        CHECK(result.has_value());

        const auto& cpu = *result;

        // 2 封装
        CHECK(cpu.packages.size() == 2);
        // 4 物理核
        CHECK(cpu.cores.size() == 4);
        // 4 逻辑 CPU
        CHECK(cpu.logical_cpus.size() == 4);

        // 封装属性
        CHECK(cpu.packages[0].physical_cores == 2);
        CHECK(cpu.packages[0].logical_threads == 2);
        CHECK(cpu.packages[1].physical_cores == 2);
        CHECK(cpu.packages[1].logical_threads == 2);

        // 型号名称
        CHECK(cpu.packages[0].model_name.value == "Intel(R) Xeon(R) Gold 6330");
        CHECK(cpu.packages[0].vendor.value == "GenuineIntel");

        // ISA 扩展
        CHECK(cpu.isa_extensions.size() == 5); // sse4_2, avx, avx2, avx512f, avx512cd
        CHECK(cpu.arch == Arch::X86_64);

        // 逻辑 CPU 可见性
        for(const auto& lc : cpu.logical_cpus)
        {
            CHECK(lc.visible_to_current_process == true);
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
        CHECK(result.has_value());

        const auto& cpu = *result;
        // 所有 CPU 归入封装 0
        CHECK(cpu.packages.size() == 1);
        CHECK(cpu.packages[0].logical_threads == 2);
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
        CHECK(result.has_value());

        const auto& cpu = *result;
        // core_id 默认为 processor 编号，所以 2 个不同的核
        CHECK(cpu.cores.size() == 2);
        CHECK(cpu.logical_cpus.size() == 2);
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
        CHECK(result.has_value());

        const auto& cpu = *result;
        // NUMA 节点
        CHECK(cpu.numa_nodes.size() == 2);
        CHECK(cpu.numa_nodes[0].id == NumaNodeId{0});
        CHECK(cpu.numa_nodes[0].cpus.size() == 2);
        CHECK(cpu.numa_nodes[1].id == NumaNodeId{1});
        CHECK(cpu.numa_nodes[1].cpus.size() == 2);

        // LogicalCpu 的 numa_node
        CHECK(cpu.logical_cpus[0].numa_node.has_value());
        CHECK(*cpu.logical_cpus[0].numa_node == NumaNodeId{0});
        CHECK(cpu.logical_cpus[2].numa_node.has_value());
        CHECK(*cpu.logical_cpus[2].numa_node == NumaNodeId{1});

        // CpuCore 的 numa_node
        CHECK(cpu.cores[0].numa_node.has_value());
        CHECK(*cpu.cores[0].numa_node == NumaNodeId{0});
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
        CHECK(result.has_value());

        const auto& cpu = *result;
        CHECK(cpu.packages[0].base_frequency.has_value());
        // 2300000 kHz → 2300000000 Hz
        CHECK(cpu.packages[0].base_frequency->value == 2300000ULL * 1000);
        CHECK(cpu.packages[0].max_frequency.has_value());
        CHECK(cpu.packages[0].max_frequency->value == 3300000ULL * 1000);
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
        CHECK(result.has_value());

        const auto& cpu = *result;
        CHECK(cpu.packages.size() == 2);

        // Package 0: 2400000 kHz → 2400000000 Hz
        CHECK(cpu.packages[0].base_frequency.has_value());
        CHECK(cpu.packages[0].base_frequency->value == 2400000ULL * 1000);
        CHECK(cpu.packages[0].max_frequency.has_value());
        CHECK(cpu.packages[0].max_frequency->value == 3500000ULL * 1000);

        // Package 1: 1800000 kHz → 1800000000 Hz (DIFFERENT from package 0)
        CHECK(cpu.packages[1].base_frequency.has_value());
        CHECK(cpu.packages[1].base_frequency->value == 1800000ULL * 1000);
        CHECK(cpu.packages[1].max_frequency.has_value());
        CHECK(cpu.packages[1].max_frequency->value == 2900000ULL * 1000);

        // Verify they are actually different
        CHECK(cpu.packages[0].base_frequency->value != cpu.packages[1].base_frequency->value);
        CHECK(cpu.packages[0].max_frequency->value != cpu.packages[1].max_frequency->value);
    }

    // ---- 测试 6: 缺少 /proc/cpuinfo 数据 ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_cpu(raw, warnings);
        CHECK(!result.has_value());
        CHECK(!warnings.empty());
    }

    // ---- 测试 8: 全部 ISA 扩展解析 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(
            RawSource::ProcCpuInfo, "/proc/cpuinfo",
            "processor\t: 0\n"
            "physical id\t: 0\n"
            "core id\t\t: 0\n"
            "flags\t\t: sse sse2 sse3 ssse3 sse4_1 sse4_2 aes fma f16c avx avx2 avx512f "
            "avx512cd avx512bw avx512dq avx512vl pclmulqdq\n"));

        std::vector<std::string> warnings;
        auto result = parse_cpu(raw, warnings);
        CHECK(result.has_value());

        const auto& cpu = *result;
        CHECK(cpu.isa_extensions.size() == 17);
        CHECK(cpu.isa_extensions[0] == IsaExtension::Sse);
        CHECK(cpu.isa_extensions[1] == IsaExtension::Sse2);
        CHECK(cpu.isa_extensions[2] == IsaExtension::Sse3);
        CHECK(cpu.isa_extensions[3] == IsaExtension::Ssse3);
        CHECK(cpu.isa_extensions[4] == IsaExtension::Sse41);
        CHECK(cpu.isa_extensions[5] == IsaExtension::Sse42);
        CHECK(cpu.isa_extensions[6] == IsaExtension::Avx);
        CHECK(cpu.isa_extensions[7] == IsaExtension::Avx2);
        CHECK(cpu.isa_extensions[8] == IsaExtension::Avx512f);
        CHECK(cpu.isa_extensions[9] == IsaExtension::Avx512cd);
        CHECK(cpu.isa_extensions[10] == IsaExtension::Avx512bw);
        CHECK(cpu.isa_extensions[11] == IsaExtension::Avx512dq);
        CHECK(cpu.isa_extensions[12] == IsaExtension::Avx512vl);
        CHECK(cpu.isa_extensions[13] == IsaExtension::Aes);
        CHECK(cpu.isa_extensions[14] == IsaExtension::Fma);
        CHECK(cpu.isa_extensions[15] == IsaExtension::F16c);
        CHECK(cpu.isa_extensions[16] == IsaExtension::Pclmulqdq);
    }

    TEST_SUMMARY();
}
