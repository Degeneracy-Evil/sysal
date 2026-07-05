#include "parser/memory.hpp"

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
        CHECK(result.has_value());

        const auto& mem = *result;
        // 395617072 kB → 395617072 * 1024 bytes
        CHECK(mem.total_memory.value == 395617072ULL * 1024);
        CHECK(mem.available_memory.has_value());
        CHECK(mem.available_memory->value == 360924620ULL * 1024);
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
        CHECK(result.has_value());

        const auto& mem = *result;
        CHECK(mem.numa_memory.size() == 2);
        CHECK(mem.numa_memory[0].node == NumaNodeId{0});
        CHECK(mem.numa_memory[0].total.value == 197808536ULL * 1024);
        CHECK(mem.numa_memory[0].available.has_value());
        CHECK(mem.numa_memory[0].available->value == 180462310ULL * 1024);
        CHECK(mem.numa_memory[1].node == NumaNodeId{1});
    }

    // ---- 测试 3: 缺少 MemAvailable ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcMemInfo, "/proc/meminfo",
                                          "MemTotal:       395617072 kB\n"
                                          "MemFree:        350000000 kB\n"));

        std::vector<std::string> warnings;
        auto result = parse_memory(raw, warnings);
        CHECK(result.has_value());
        CHECK(!result->available_memory.has_value());
    }

    // ---- 测试 4: 缺少 /proc/meminfo 数据 ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_memory(raw, warnings);
        CHECK(!result.has_value());
        CHECK(!warnings.empty());
    }

    // ---- 测试 5: MemTotal 为 0 ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::ProcMemInfo, "/proc/meminfo", "MemTotal:             0 kB\n"));

        std::vector<std::string> warnings;
        auto result = parse_memory(raw, warnings);
        CHECK(!result.has_value());
    }

    // ---- 测试 6: udevadm 解析 DIMM（一个已安装、一个空槽）----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcMemInfo, "/proc/meminfo",
                                          "MemTotal:       34359738368 kB\n"
                                          "MemAvailable:   30000000000 kB\n"));
        raw.records.push_back(make_record(RawSource::Udevadm, "udevadm info -e",
                                          "P: /devices/virtual/dmi/id\n"
                                          "E: ID_VENDOR=Supermicro\n"
                                          "E: MEMORY_DEVICE_0_TYPE=DDR4\n"
                                          "E: MEMORY_DEVICE_0_SPEED_MTS=3200\n"
                                          "E: MEMORY_DEVICE_0_CONFIGURED_SPEED_MTS=2933\n"
                                          "E: MEMORY_DEVICE_0_SIZE=34359738368\n"
                                          "E: MEMORY_DEVICE_0_MANUFACTURER=Samsung\n"
                                          "E: MEMORY_DEVICE_0_PART_NUMBER=M393A4K40EB3-CWE\n"
                                          "E: MEMORY_DEVICE_0_SERIAL=T0HA00014848D5609A\n"
                                          "E: MEMORY_DEVICE_0_LOCATOR=CPU0_C0D0\n"
                                          "E: MEMORY_DEVICE_0_BANK_LOCATOR=NODE 0\n"
                                          "E: MEMORY_DEVICE_0_RANK=2\n"
                                          "E: MEMORY_DEVICE_0_TOTAL_WIDTH=72\n"
                                          "E: MEMORY_DEVICE_0_DATA_WIDTH=64\n"
                                          "E: MEMORY_DEVICE_0_FORM_FACTOR=DIMM\n"
                                          "E: MEMORY_DEVICE_0_PRESENT=1\n"
                                          "E: MEMORY_DEVICE_1_PRESENT=0\n"
                                          "E: MEMORY_DEVICE_1_LOCATOR=CPU0_C0D1\n"
                                          "E: MEMORY_DEVICE_1_BANK_LOCATOR=NODE 0\n"));

        std::vector<std::string> warnings;
        auto result = parse_memory(raw, warnings);
        CHECK(result.has_value());

        const auto& mem = *result;
        CHECK(mem.dimms.size() == 2);
        CHECK(mem.dimm_count.has_value());
        CHECK(*mem.dimm_count == 2);
        CHECK(mem.populated_dimms.has_value());
        CHECK(*mem.populated_dimms == 1);

        const auto& d0 = mem.dimms[0];
        CHECK(d0.present);
        CHECK(d0.type == "DDR4");
        CHECK(d0.locator == "CPU0_C0D0");
        CHECK(d0.bank_locator == "NODE 0");
        CHECK(d0.size.value == 34359738368ULL);
        CHECK(d0.speed_mts.has_value());
        CHECK(d0.speed_mts->value == 3200);
        CHECK(d0.configured_speed_mts.has_value());
        CHECK(d0.configured_speed_mts->value == 2933);
        CHECK(d0.manufacturer.has_value());
        CHECK(d0.manufacturer->value == "Samsung");
        CHECK(d0.part_number.has_value());
        CHECK(*d0.part_number == "M393A4K40EB3-CWE");
        CHECK(d0.rank.has_value());
        CHECK(*d0.rank == 2);
        CHECK(d0.total_width.has_value());
        CHECK(*d0.total_width == 72);
        CHECK(d0.data_width.has_value());
        CHECK(*d0.data_width == 64);
        CHECK(d0.form_factor.has_value());
        CHECK(*d0.form_factor == "DIMM");

        const auto& d1 = mem.dimms[1];
        CHECK(!d1.present);
        CHECK(d1.locator == "CPU0_C0D1");
        CHECK(d1.bank_locator == "NODE 0");
        CHECK(d1.size.value == 0);
        CHECK(d1.type.empty());
    }

    // ---- 测试 7: EDAC 回退解析 DIMM ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcMemInfo, "/proc/meminfo",
                                          "MemTotal:       32768000 kB\n"
                                          "MemAvailable:   30000000 kB\n"));
        raw.records.push_back(make_record(RawSource::SysfsEdac,
                                          "/sys/devices/system/edac/mc/mc0/dimm0/dimm_mem_type",
                                          "Unbuffered-DDR4\n"));
        raw.records.push_back(make_record(RawSource::SysfsEdac,
                                          "/sys/devices/system/edac/mc/mc0/dimm0/size", "32768\n"));
        raw.records.push_back(make_record(RawSource::SysfsEdac,
                                          "/sys/devices/system/edac/mc/mc0/dimm0/dimm_label",
                                          "CPU_SrcID#0_MC#0_Chan#0_DIMM#0\n"));
        raw.records.push_back(make_record(RawSource::SysfsEdac,
                                          "/sys/devices/system/edac/mc/mc0/dimm0/dimm_location",
                                          "channel 0 slot 0\n"));
        raw.records.push_back(make_record(RawSource::SysfsEdac,
                                          "/sys/devices/system/edac/mc/mc0/dimm1/dimm_mem_type",
                                          "Unbuffered-DDR4\n"));
        raw.records.push_back(make_record(RawSource::SysfsEdac,
                                          "/sys/devices/system/edac/mc/mc0/dimm1/size", "32768\n"));
        raw.records.push_back(make_record(RawSource::SysfsEdac,
                                          "/sys/devices/system/edac/mc/mc0/dimm1/dimm_label",
                                          "CPU_SrcID#0_MC#0_Chan#1_DIMM#0\n"));
        raw.records.push_back(make_record(RawSource::SysfsEdac,
                                          "/sys/devices/system/edac/mc/mc0/dimm1/dimm_location",
                                          "channel 1 slot 0\n"));

        std::vector<std::string> warnings;
        auto result = parse_memory(raw, warnings);
        CHECK(result.has_value());

        const auto& mem = *result;
        CHECK(mem.dimms.size() == 2);
        CHECK(mem.dimm_count.has_value());
        CHECK(*mem.dimm_count == 2);
        CHECK(mem.populated_dimms.has_value());
        CHECK(*mem.populated_dimms == 2);

        const auto& d0 = mem.dimms[0];
        CHECK(d0.present);
        CHECK(d0.type == "Unbuffered-DDR4");
        CHECK(d0.size.value == 32768ULL * 1024 * 1024);
        CHECK(d0.locator == "CPU_SrcID#0_MC#0_Chan#0_DIMM#0");
        CHECK(d0.bank_locator == "channel 0 slot 0");

        const auto& d1 = mem.dimms[1];
        CHECK(d1.present);
        CHECK(d1.type == "Unbuffered-DDR4");
        CHECK(d1.size.value == 32768ULL * 1024 * 1024);
        CHECK(d1.locator == "CPU_SrcID#0_MC#0_Chan#1_DIMM#0");
    }

    // ---- 测试 8: 无 udevadm 无 EDAC，dimms 为空 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcMemInfo, "/proc/meminfo",
                                          "MemTotal:       16777216 kB\n"
                                          "MemAvailable:   15000000 kB\n"));

        std::vector<std::string> warnings;
        auto result = parse_memory(raw, warnings);
        CHECK(result.has_value());

        const auto& mem = *result;
        CHECK(mem.dimms.empty());
        CHECK(!mem.dimm_count.has_value());
        CHECK(!mem.populated_dimms.has_value());
    }

    // ---- 测试 9: udevadm 畸形输入（缺字段名、不可解析索引）不崩溃且产生警告 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcMemInfo, "/proc/meminfo",
                                          "MemTotal:       34359738368 kB\n"
                                          "MemAvailable:   30000000000 kB\n"));
        raw.records.push_back(make_record(RawSource::Udevadm, "udevadm info -e",
                                          "E: MEMORY_DEVICE_0_TYPE=DDR4\n"
                                          "E: MEMORY_DEVICE_0_SIZE=34359738368\n"
                                          "E: MEMORY_DEVICE_0_PRESENT=1\n"
                                          "E: MEMORY_DEVICE_0_LOCATOR=CPU0_C0D0\n"
                                          "E: MEMORY_DEVICE_abc_TYPE=DDR4\n"
                                          "E: MEMORY_DEVICE_1_\n"
                                          "E: NOT_MEMORY_DEVICE=ignore\n"
                                          "garbage_line_no_colon\n"));

        std::vector<std::string> warnings;
        auto result = parse_memory(raw, warnings);
        CHECK(result.has_value());

        const auto& mem = *result;
        CHECK(mem.dimms.size() == 1);
        CHECK(mem.dimm_count.has_value());
        CHECK(*mem.dimm_count == 1);
        CHECK(mem.populated_dimms.has_value());
        CHECK(*mem.populated_dimms == 1);
        CHECK(mem.dimms[0].type == "DDR4");
        CHECK(mem.dimms[0].size.value == 34359738368ULL);
        CHECK(mem.dimms[0].present);
        bool has_warning = false;
        for(const auto& w : warnings)
        {
            if(w.find("parse_udevadm_dimms") != std::string::npos)
            {
                has_warning = true;
                break;
            }
        }
        CHECK(has_warning);
    }

    TEST_SUMMARY();
}
