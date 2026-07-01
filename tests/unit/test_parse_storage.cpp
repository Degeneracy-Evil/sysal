#include "parser/storage.hpp"

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
    // ---- 测试 1: 2 块设备（nvme0n1 + sda，sda 为 HDD） ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::SysfsBlock, "/sys/block/nvme0n1/size", "3750924672\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsBlock, "/sys/block/nvme0n1/queue/rotational", "0\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsBlock, "/sys/block/sda/size", "976773168\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsBlock, "/sys/block/sda/queue/rotational", "1\n"));

        std::vector<std::string> warnings;
        auto result = parse_storage(raw, warnings);
        CHECK(result.has_value());

        const auto& stor = *result;
        CHECK(stor.devices.size() == 2);

        // nvme0n1
        CHECK(stor.devices[0].id == StorageId{0});
        CHECK(stor.devices[0].name.value == "nvme0n1");
        CHECK(stor.devices[0].kind == StorageKind::Nvme);
        CHECK(stor.devices[0].capacity.has_value());
        CHECK(stor.devices[0].capacity->value == 3750924672ULL * 512);

        // sda (rotational=1 → Hdd)
        CHECK(stor.devices[1].id == StorageId{1});
        CHECK(stor.devices[1].name.value == "sda");
        CHECK(stor.devices[1].kind == StorageKind::Hdd);
        CHECK(stor.devices[1].capacity.has_value());
        CHECK(stor.devices[1].capacity->value == 976773168ULL * 512);
    }

    // ---- 测试 2: 空 SysfsBlock → nullopt ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_storage(raw, warnings);
        CHECK(!result.has_value());
    }

    // ---- 测试 3: 未知设备类型 → Other ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::SysfsBlock, "/sys/block/loop0/size", "0\n"));

        std::vector<std::string> warnings;
        auto result = parse_storage(raw, warnings);
        CHECK(result.has_value());

        const auto& stor = *result;
        CHECK(stor.devices.size() == 1);
        CHECK(stor.devices[0].name.value == "loop0");
        CHECK(stor.devices[0].kind == StorageKind::Other);
    }

    // ---- 测试 4: 单设备无 rotational ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::SysfsBlock, "/sys/block/nvme0n1/size", "1000\n"));

        std::vector<std::string> warnings;
        auto result = parse_storage(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->devices.size() == 1);
        CHECK(result->devices[0].name.value == "nvme0n1");
        CHECK(result->devices[0].kind == StorageKind::Nvme);
        CHECK(result->devices[0].capacity.has_value());
    }

    // ---- 测试 5: rotational=0 → Ssd ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::SysfsBlock, "/sys/block/sdb/size", "500118192\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsBlock, "/sys/block/sdb/queue/rotational", "0\n"));

        std::vector<std::string> warnings;
        auto result = parse_storage(raw, warnings);
        CHECK(result.has_value());

        const auto& stor = *result;
        CHECK(stor.devices.size() == 1);
        CHECK(stor.devices[0].name.value == "sdb");
        CHECK(stor.devices[0].kind == StorageKind::Ssd);
    }

    // ---- 测试 6: nvme 设备无 rotational → Nvme ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::SysfsBlock, "/sys/block/nvme1n1/size", "1000\n"));

        std::vector<std::string> warnings;
        auto result = parse_storage(raw, warnings);
        CHECK(result.has_value());

        const auto& stor = *result;
        CHECK(stor.devices.size() == 1);
        CHECK(stor.devices[0].name.value == "nvme1n1");
        CHECK(stor.devices[0].kind == StorageKind::Nvme);
    }

    // ---- 测试 7: sd 设备无 rotational → Other ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::SysfsBlock, "/sys/block/sdc/size", "2000\n"));

        std::vector<std::string> warnings;
        auto result = parse_storage(raw, warnings);
        CHECK(result.has_value());

        const auto& stor = *result;
        CHECK(stor.devices.size() == 1);
        CHECK(stor.devices[0].name.value == "sdc");
        CHECK(stor.devices[0].kind == StorageKind::Other);
    }

    TEST_SUMMARY();
}
