/// @file test_raw_store_io.cpp
/// @brief RawStore JSON 序列化往返测试

#include "sysal/core/error.hpp"
#include "sysal/model/raw_store.hpp"
#include "sysal/test/replay.hpp"
#include "sysal/types/enums.hpp"

#include "test_macros.hpp"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

/// @brief 构造测试用 RawStore（3 条不同来源的记录）
[[nodiscard]] sysal::RawStore make_test_store()
{
    auto now = std::chrono::system_clock::now();
    sysal::RawStore store;
    store.records.push_back(sysal::RawRecord{sysal::RawSource::ProcCpuInfo, "/proc/cpuinfo",
                                             "processor\t: 0\nmodel name\t: Intel(R) Xeon(R)",
                                             sysal::CollectStatus::Success, now});
    store.records.push_back(sysal::RawRecord{sysal::RawSource::ProcMemInfo, "/proc/meminfo",
                                             "MemTotal:       32768000 kB",
                                             sysal::CollectStatus::Success, now});
    store.records.push_back(sysal::RawRecord{sysal::RawSource::Lspci, "lspci -mm",
                                             "00:01.0 \"PCI bridge\" \"Intel\" \"Device 1234\"",
                                             sysal::CollectStatus::Partial, now});
    return store;
}

/// @brief 比较两个 RawRecord 是否相等
[[nodiscard]] bool record_equal(const sysal::RawRecord& a, const sysal::RawRecord& b)
{
    if(a.source != b.source || a.path_or_command != b.path_or_command || a.payload != b.payload ||
       a.status != b.status)
    {
        return false;
    }
    // 时间戳精度为毫秒，比较 epoch 毫秒
    auto a_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(a.collected_at.time_since_epoch())
            .count();
    auto b_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(b.collected_at.time_since_epoch())
            .count();
    return a_ms == b_ms;
}

/// @brief 生成临时文件路径
[[nodiscard]] std::string temp_file_path()
{
    return "/tmp/sysal_test_raw_store_io_" +
           std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count()) +
           ".json";
}

} // namespace

/// @brief 测试 save → load 往返一致性
void test_roundtrip()
{
    auto original = make_test_store();
    auto path = temp_file_path();

    sysal::test::save_raw_store(original, path);
    auto loaded = sysal::test::load_raw_store(path);

    CHECK(loaded.records.size() == original.records.size());
    for(std::size_t i = 0; i < original.records.size(); ++i)
    {
        CHECK(record_equal(original.records[i], loaded.records[i]));
    }

    std::remove(path.c_str());
    std::cout << "  roundtrip: OK\n";
}

/// @brief 测试加载不存在的文件抛出 SysalError
void test_load_nonexistent()
{
    try
    {
        sysal::test::load_raw_store("/tmp/sysal_nonexistent_file_12345.json");
        CHECK(false && "expected SysalError");
    }
    catch(const sysal::SysalError& e)
    {
        CHECK(e.kind() == sysal::ErrorKind::FileNotFound);
    }

    std::cout << "  load nonexistent: OK\n";
}

/// @brief 测试加载畸形 JSON 抛出 SysalError
void test_load_invalid_json()
{
    auto path = temp_file_path();
    {
        std::ofstream ofs(path);
        ofs << "{invalid json";
    }

    try
    {
        sysal::test::load_raw_store(path);
        CHECK(false && "expected SysalError");
    }
    catch(const sysal::SysalError& e)
    {
        CHECK(e.kind() == sysal::ErrorKind::DeserializationError);
    }

    std::remove(path.c_str());
    std::cout << "  load invalid json: OK\n";
}

/// @brief 测试空 RawStore 往返
void test_empty_store()
{
    sysal::RawStore empty;
    auto path = temp_file_path();

    sysal::test::save_raw_store(empty, path);
    auto loaded = sysal::test::load_raw_store(path);

    CHECK(loaded.records.empty());

    std::remove(path.c_str());
    std::cout << "  empty store: OK\n";
}

int main()
{
    std::cout << "test_raw_store_io:\n";
    test_roundtrip();
    test_load_nonexistent();
    test_load_invalid_json();
    test_empty_store();
    std::cout << "  all passed!\n";
    TEST_SUMMARY();
}
