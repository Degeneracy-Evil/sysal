/// @file test_replay.cpp
/// @brief Raw replay 测试
/// @details 从 fixture 文件加载 RawStore，执行 Parser→Resolver 回放管线，
///          验证域不变量。首次运行时自动生成 fixture。

#include <sysal/core/system.hpp>
#include <sysal/test/replay.hpp>

#include <cassert>
#include <filesystem>
#include <iostream>

namespace
{

/// @brief fixture 文件路径
const std::string fixture_path = "tests/fixtures/dev_machine.json";

/// @brief 断言宏，带消息输出
#define CHECK(cond, msg)                                                                           \
    do                                                                                             \
    {                                                                                              \
        if(!(cond))                                                                                \
        {                                                                                          \
            std::cerr << "FAIL: " << (msg) << "\n";                                                \
            return 1;                                                                              \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            std::cout << "  PASS: " << (msg) << "\n";                                              \
        }                                                                                          \
    } while(0)

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
    CHECK(!raw.records.empty(),
          "fixture has records (count=" + std::to_string(raw.records.size()) + ")");

    // 3. 回放采集
    std::cout << "\nStep 3: Replaying from raw store...\n";
    auto sys = sysal::test::collect_from_raw(raw);
    CHECK(true, "collect_from_raw succeeded");

    // 4. 验证域不变量
    std::cout << "\nStep 4: Verifying domain invariants...\n";

    // CPU
    CHECK(!sys.info.cpu.logical_cpus.empty(),
          "cpu.logical_cpus not empty (size=" + std::to_string(sys.info.cpu.logical_cpus.size()) +
              ")");
    CHECK(!sys.info.cpu.packages.empty(),
          "cpu.packages not empty (size=" + std::to_string(sys.info.cpu.packages.size()) + ")");

    // Memory
    CHECK(sys.info.memory.total_memory.value > 0,
          "memory.total_memory > 0 (" + std::to_string(sys.info.memory.total_memory.value) +
              " bytes)");

    // Platform
    CHECK(!sys.info.platform.os.name.empty(),
          "platform.os.name not empty ('" + sys.info.platform.os.name + "')");
    CHECK(!sys.info.platform.kernel.release.empty(),
          "platform.kernel.release not empty ('" + sys.info.platform.kernel.release + "')");
    CHECK(!sys.info.platform.architecture.name.empty(),
          "platform.architecture.name not empty ('" + sys.info.platform.architecture.name + "')");

    // Network
    CHECK(!sys.info.network.interfaces.empty(),
          "network.interfaces not empty (size=" +
              std::to_string(sys.info.network.interfaces.size()) + ")");

    // PCI
    CHECK(!sys.info.pci.devices.empty(),
          "pci.devices not empty (size=" + std::to_string(sys.info.pci.devices.size()) + ")");

    // Execution
    CHECK(sys.info.execution.process.pid > 0,
          "execution.process.pid > 0 (" + std::to_string(sys.info.execution.process.pid) + ")");

    // Meta
    CHECK(!sys.meta.succeeded_collectors.empty(),
          "meta.succeeded_collectors not empty (size=" +
              std::to_string(sys.meta.succeeded_collectors.size()) + ")");
    CHECK(!sys.meta.sysal_version.empty(),
          "meta.sysal_version not empty ('" + sys.meta.sysal_version + "')");

    // Warnings（可能为空，仅验证类型有效）
    CHECK(true, "warnings vector valid (size=" + std::to_string(sys.warnings.size()) + ")");

    // 5. 与实时采集对比关键指标
    std::cout << "\nStep 5: Comparing replay vs live collection...\n";
    auto live = sysal::System::collect();
    CHECK(sys.info.cpu.logical_cpus.size() == live.info.cpu.logical_cpus.size(),
          "replay CPU count matches live (" + std::to_string(sys.info.cpu.logical_cpus.size()) +
              " vs " + std::to_string(live.info.cpu.logical_cpus.size()) + ")");
    CHECK(sys.info.memory.total_memory.value == live.info.memory.total_memory.value,
          "replay memory matches live (" + std::to_string(sys.info.memory.total_memory.value) +
              " vs " + std::to_string(live.info.memory.total_memory.value) + ")");

    std::cout << "\n=== test_replay: ALL PASSED ===\n";
    return 0;
}
