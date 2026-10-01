/// @file test_replay.cpp
/// @brief Raw replay 测试
/// @details 从 fixture 文件加载 RawStore，执行 Parser→Resolver 回放管线，
///          验证域不变量，不修改 fixture。

#include "sysal/core/system.hpp"
#include "sysal/test/replay.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <iostream>

namespace
{

    /// @brief fixture 文件路径
    const std::string fixture_path = "tests/fixtures/dev_machine.json";

} // namespace

TEST_CASE("test_replay")
{
    std::cout << "=== test_replay ===\n\n";

    // 回放只读取已提交的 fixture，不生成或修改仓库文件。
    CHECK(std::filesystem::exists(fixture_path));

    // 2. 加载 fixture
    std::cout << "\nStep 2: Loading fixture...\n";
    auto raw = sysal::test::load_raw_store(fixture_path);
    CHECK(!raw.records.empty());

    // 3. 回放采集
    std::cout << "\nStep 3: Replaying from raw store...\n";
    auto sys = sysal::test::collect_from_raw(raw);
    CHECK(!sys.info.platform.host.hostname.empty());

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

    std::cout << "\n=== test_replay: completed ===\n";
}
