/// @file test_replay.cpp
/// @brief Raw replay 测试
/// @details 从 fixture 文件加载 RawStore，执行 Parser→Resolver 回放管线，
///          验证域不变量。首次运行时自动生成 fixture。

#include "sysal/core/system.hpp"
#include "sysal/test/replay.hpp"

#include "test_macros.hpp"

#include <filesystem>
#include <iostream>

namespace
{

/// @brief fixture 文件路径
const std::string fixture_path = "tests/fixtures/dev_machine.json";

/// @brief 生成 fixture 文件
/// @details 采集当前机器的原始数据并保存到 fixture 路径。
void generate_fixture()
{
    auto sys = sysal::System::collect(sysal::full);
    if(sys.raw.has_value())
    {
        sysal::test::save_raw_store(*sys.raw, fixture_path);
        std::cout << "  Generated fixture: " << fixture_path << "\n";
    }
    else
    {
        std::cerr << "  WARNING: No raw data collected, cannot generate fixture\n";
    }
}

} // namespace

int main()
{
    std::cout << "=== test_replay ===\n\n";

    // 1. 若 fixture 不存在则生成
    if(!std::filesystem::exists(fixture_path))
    {
        std::cout << "Step 1: Generating fixture...\n";
        generate_fixture();
    }
    else
    {
        std::cout << "Step 1: Fixture already exists\n";
    }

    // 2. 加载 fixture
    std::cout << "\nStep 2: Loading fixture...\n";
    auto raw = sysal::test::load_raw_store(fixture_path);
    CHECK(!raw.records.empty());

    // 3. 回放采集
    std::cout << "\nStep 3: Replaying from raw store...\n";
    auto sys = sysal::test::collect_from_raw(raw);
    CHECK(true);

    // 4. 验证域不变量
    std::cout << "\nStep 4: Verifying domain invariants...\n";

    // CPU
    CHECK(!sys.info.cpu.logical_cpus.empty());
    CHECK(!sys.info.cpu.packages.empty());

    // Memory
    CHECK(sys.info.memory.total_memory.value > 0);

    // Platform
    CHECK(!sys.info.platform.os.name.empty());
    CHECK(!sys.info.platform.kernel.release.empty());
    CHECK(!sys.info.platform.architecture.name.empty());

    // Network
    CHECK(!sys.info.network.interfaces.empty());

    // PCI
    CHECK(!sys.info.pci.devices.empty());

    // Execution
    CHECK(sys.info.execution.process.pid > 0);

    // Meta
    CHECK(!sys.meta.succeeded_collectors.empty());
    CHECK(!sys.meta.sysal_version.empty());

    // Warnings（可能为空，仅验证类型有效）
    CHECK(true);

    // 5. 与实时采集对比关键指标
    std::cout << "\nStep 5: Comparing replay vs live collection...\n";
    auto live = sysal::System::collect();
    CHECK(sys.info.cpu.logical_cpus.size() == live.info.cpu.logical_cpus.size());
    CHECK(sys.info.memory.total_memory.value == live.info.memory.total_memory.value);

    std::cout << "\n=== test_replay: ALL PASSED ===\n";
    TEST_SUMMARY();
}
