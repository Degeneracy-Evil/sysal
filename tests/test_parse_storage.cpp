#include "parser/storage.hpp"

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
    // ---- 测试 1: 2 块设备（nvme0n1 + sda） ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::SysfsBlock, "/sys/block/nvme0n1/size", "3750924672\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsBlock, "/sys/block/sda/size", "976773168\n"));

        std::vector<std::string> warnings;
        auto result = parse_storage(raw, warnings);
        assert(result.has_value());

        const auto& stor = *result;
        assert(stor.devices.size() == 2);

        // nvme0n1
        assert(stor.devices[0].id == StorageId{0});
        assert(stor.devices[0].name.value == "nvme0n1");
        assert(stor.devices[0].kind == StorageKind::Nvme);
        assert(stor.devices[0].capacity.has_value());
        assert(stor.devices[0].capacity->value == 3750924672ULL * 512);

        // sda
        assert(stor.devices[1].id == StorageId{1});
        assert(stor.devices[1].name.value == "sda");
        assert(stor.devices[1].kind == StorageKind::Sata);
        assert(stor.devices[1].capacity.has_value());
        assert(stor.devices[1].capacity->value == 976773168ULL * 512);
    }

    // ---- 测试 2: 空 SysfsBlock → nullopt ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_storage(raw, warnings);
        assert(!result.has_value());
    }

    // ---- 测试 3: 未知设备类型 → Other ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::SysfsBlock, "/sys/block/loop0/size", "0\n"));

        std::vector<std::string> warnings;
        auto result = parse_storage(raw, warnings);
        assert(result.has_value());

        const auto& stor = *result;
        assert(stor.devices.size() == 1);
        assert(stor.devices[0].name.value == "loop0");
        assert(stor.devices[0].kind == StorageKind::Other);
    }

    // ---- 测试 4: PCI 地址缺失警告（B-2） ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::SysfsBlock, "/sys/block/nvme0n1/size", "1000\n"));

        std::vector<std::string> warnings;
        auto result = parse_storage(raw, warnings);
        assert(result.has_value());

        // 应有 B-2 警告
        bool has_b2_warning = false;
        for(const auto& w : warnings)
        {
            if(w.find("B-2") != std::string::npos)
            {
                has_b2_warning = true;
                break;
            }
        }
        assert(has_b2_warning);
    }

    return 0;
}
