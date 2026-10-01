#include "parser/accelerator.hpp"

#include "sysal/model/raw_store.hpp"
#include "sysal/types/enums.hpp"

#include <chrono>
#include <doctest/doctest.h>
#include <string>
#include <vector>

using namespace sysal;
using namespace sysal::detail;

/// @brief 创建一条 RawRecord
static RawRecord make_record(RawSource source, const std::string &path, const std::string &payload)
{
    return RawRecord{source, path, payload, CollectStatus::Success, std::chrono::system_clock::now()};
}

TEST_CASE("test_parse_accelerator")
{
    // ---- 测试 1: 2 块 NVIDIA H20 GPU ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::NvidiaSmi, "nvidia-smi",
                                          "index, name, memory.total, pci.bus_id, driver_version\n"
                                          "0, NVIDIA H20 96GB, 97536 MiB, 00000000:41:00.0, 535.129.03\n"
                                          "1, NVIDIA H20 96GB, 97536 MiB, 00000000:42:00.0, 535.129.03\n"));

        std::vector<std::string> warnings;
        auto result = parse_accelerator(raw, warnings);
        REQUIRE(result.has_value());
        if(!result.has_value())
        {
            return;
        }

        const auto &acc = *result;
        CHECK(acc.devices.size() == 2);

        // 设备 0
        CHECK(acc.devices[0].id == AcceleratorId{0});
        CHECK(acc.devices[0].kind == AcceleratorKind::Gpu);
        CHECK(acc.devices[0].vendor.value == "NVIDIA");
        CHECK(acc.devices[0].name.value == "NVIDIA H20 96GB");
        const auto &pci_address_1 = acc.devices[0].pci_address;
        REQUIRE(pci_address_1.has_value());
        if(!pci_address_1.has_value())
        {
            return;
        }
        CHECK(pci_address_1->domain == 0);
        CHECK(pci_address_1->bus == 0x41);
        CHECK(pci_address_1->device == 0x00);
        CHECK(pci_address_1->function == 0x0);
        const auto &memory_size_2 = acc.devices[0].memory_size;
        REQUIRE(memory_size_2.has_value());
        if(!memory_size_2.has_value())
        {
            return;
        }
        CHECK(memory_size_2->value == 97536ULL * 1024ULL * 1024ULL);
        CHECK(acc.devices[0].visible_to_current_process == true);
        CHECK(!acc.devices[0].driver.has_value());

        // 设备 1
        CHECK(acc.devices[1].id == AcceleratorId{1});
        CHECK(acc.devices[1].kind == AcceleratorKind::Gpu);
        CHECK(acc.devices[1].name.value == "NVIDIA H20 96GB");
        const auto &pci_address_3 = acc.devices[1].pci_address;
        REQUIRE(pci_address_3.has_value());
        if(!pci_address_3.has_value())
        {
            return;
        }
        CHECK(pci_address_3->bus == 0x42);
        const auto &memory_size_4 = acc.devices[1].memory_size;
        REQUIRE(memory_size_4.has_value());
        if(!memory_size_4.has_value())
        {
            return;
        }
        CHECK(memory_size_4->value == 97536ULL * 1024ULL * 1024ULL);
    }

    // ---- 测试 2: NUMA 节点查找（D-4 修正） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::NvidiaSmi, "nvidia-smi",
                                          "index, name, memory.total, pci.bus_id, driver_version\n"
                                          "0, NVIDIA H20 96GB, 97536 MiB, 00000000:41:00.0, 535.129.03\n"));
        raw.records.push_back(make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:41:00.0/numa_node", "0\n"));

        std::vector<std::string> warnings;
        auto result = parse_accelerator(raw, warnings);
        REQUIRE(result.has_value());
        if(!result.has_value())
        {
            return;
        }

        const auto &acc = *result;
        CHECK(acc.devices.size() == 1);
        const auto &nearest_numa_node_5 = acc.devices[0].nearest_numa_node;
        REQUIRE(nearest_numa_node_5.has_value());
        if(!nearest_numa_node_5.has_value())
        {
            return;
        }
        CHECK(*nearest_numa_node_5 == NumaNodeId{0});
    }

    // ---- 测试 3: 空 NvidiaSmi → nullopt ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_accelerator(raw, warnings);
        CHECK(!result.has_value());
    }

    // ---- 测试 4: GiB 单位解析 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::NvidiaSmi, "nvidia-smi",
                                          "index, name, memory.total, pci.bus_id, driver_version\n"
                                          "0, NVIDIA A100 80GB, 80 GiB, 00000000:3b:00.0, 535.129.03\n"));

        std::vector<std::string> warnings;
        auto result = parse_accelerator(raw, warnings);
        REQUIRE(result.has_value());
        if(!result.has_value())
        {
            return;
        }

        const auto &acc = *result;
        CHECK(acc.devices.size() == 1);
        const auto &memory_size_6 = acc.devices[0].memory_size;
        REQUIRE(memory_size_6.has_value());
        if(!memory_size_6.has_value())
        {
            return;
        }
        CHECK(memory_size_6->value == 80ULL * 1024ULL * 1024ULL * 1024ULL);
    }
}
