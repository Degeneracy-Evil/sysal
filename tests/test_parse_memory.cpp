#include "parser/memory.hpp"

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
    // ---- 测试 1: 基本内存信息解析 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcMemInfo, "/proc/meminfo",
                                          "MemTotal:       395617072 kB\n"
                                          "MemFree:        350000000 kB\n"
                                          "MemAvailable:   360924620 kB\n"
                                          "Buffers:          2097152 kB\n"
                                          "Cached:          10485760 kB\n"));

        std::vector<std::string> warnings;
        auto result = parse_memory(raw, warnings);
        assert(result.has_value());

        const auto& mem = *result;
        // 395617072 kB → 395617072 * 1024 bytes
        assert(mem.total_memory.value == 395617072ULL * 1024);
        assert(mem.available_memory.has_value());
        assert(mem.available_memory->value == 360924620ULL * 1024);
    }

    // ---- 测试 2: NUMA 内存信息 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcMemInfo, "/proc/meminfo",
                                          "MemTotal:       395617072 kB\n"
                                          "MemAvailable:   360924620 kB\n"));
        raw.records.push_back(make_record(RawSource::SysfsNuma, "node/node0/meminfo",
                                          "Node 0 Total:      197808536 kB\n"
                                          "Node 0 Free:       180462310 kB\n"));
        raw.records.push_back(make_record(RawSource::SysfsNuma, "node/node1/meminfo",
                                          "Node 1 Total:      197808536 kB\n"
                                          "Node 1 Free:       180462310 kB\n"));

        std::vector<std::string> warnings;
        auto result = parse_memory(raw, warnings);
        assert(result.has_value());

        const auto& mem = *result;
        assert(mem.numa_memory.size() == 2);
        assert(mem.numa_memory[0].node == NumaNodeId{0});
        assert(mem.numa_memory[0].total.value == 197808536ULL * 1024);
        assert(mem.numa_memory[0].available.has_value());
        assert(mem.numa_memory[0].available->value == 180462310ULL * 1024);
        assert(mem.numa_memory[1].node == NumaNodeId{1});
    }

    // ---- 测试 3: 缺少 MemAvailable ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcMemInfo, "/proc/meminfo",
                                          "MemTotal:       395617072 kB\n"
                                          "MemFree:        350000000 kB\n"));

        std::vector<std::string> warnings;
        auto result = parse_memory(raw, warnings);
        assert(result.has_value());
        assert(!result->available_memory.has_value());
    }

    // ---- 测试 4: 缺少 /proc/meminfo 数据 ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_memory(raw, warnings);
        assert(!result.has_value());
        assert(!warnings.empty());
    }

    // ---- 测试 5: MemTotal 为 0 ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::ProcMemInfo, "/proc/meminfo", "MemTotal:             0 kB\n"));

        std::vector<std::string> warnings;
        auto result = parse_memory(raw, warnings);
        assert(!result.has_value());
    }

    return 0;
}
