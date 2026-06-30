#include "parser/pci.hpp"

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
    // ---- 测试 1: 两个 PCI 设备 ----
    {
        RawStore raw;
        // 设备 1: NVIDIA GPU
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:41:00.0/vendor", "0x10de"));
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:41:00.0/device", "0x2322"));
        raw.records.push_back(make_record(RawSource::SysfsPci,
                                          "/sys/bus/pci/devices/0000:41:00.0/class", "0x030000"));
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:41:00.0/numa_node", "0"));

        // 设备 2: Mellanox NIC
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:65:00.0/vendor", "0x15b3"));
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:65:00.0/device", "0x158b"));
        raw.records.push_back(make_record(RawSource::SysfsPci,
                                          "/sys/bus/pci/devices/0000:65:00.0/class", "0x020000"));
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:65:00.0/numa_node", "1"));

        std::vector<std::string> warnings;
        auto result = parse_pci(raw, warnings);
        assert(result.has_value());

        const auto& pci = *result;
        assert(pci.devices.size() == 2);

        // 设备按 map 顺序（字典序），0000:41:00.0 在前
        assert((pci.devices[0].address == PciAddress{0x0000, 0x41, 0x00, 0x0}));
        assert(pci.devices[0].vendor == Vendor{"0x10de"});
        assert(pci.devices[0].device_name == DeviceName{"0x2322"});
        assert(pci.devices[0].device_class == PciClass{"0x030000"});
        assert(pci.devices[0].numa_node.has_value());
        assert(pci.devices[0].numa_node == NumaNodeId{0});

        assert((pci.devices[1].address == PciAddress{0x0000, 0x65, 0x00, 0x0}));
        assert(pci.devices[1].vendor == Vendor{"0x15b3"});
        assert(pci.devices[1].device_name == DeviceName{"0x158b"});
        assert(pci.devices[1].device_class == PciClass{"0x020000"});
        assert(pci.devices[1].numa_node.has_value());
        assert(pci.devices[1].numa_node == NumaNodeId{1});
    }

    // ---- 测试 2: numa_node 为 -1 → nullopt ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:01:00.0/vendor", "0x8086"));
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:01:00.0/device", "0x1528"));
        raw.records.push_back(make_record(RawSource::SysfsPci,
                                          "/sys/bus/pci/devices/0000:01:00.0/class", "0x020000"));
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:01:00.0/numa_node", "-1"));

        std::vector<std::string> warnings;
        auto result = parse_pci(raw, warnings);
        assert(result.has_value());
        assert(result->devices.size() == 1);
        assert(!result->devices[0].numa_node.has_value());
    }

    // ---- 测试 3: 无 SysfsPci 数据 → nullopt ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_pci(raw, warnings);
        assert(!result.has_value());
    }

    // ---- 测试 4: 无效 PCI 地址 → 警告 ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/INVALID/vendor", "0x1234"));

        std::vector<std::string> warnings;
        auto result = parse_pci(raw, warnings);
        assert(result.has_value());
        assert(result->devices.empty());
        assert(!warnings.empty());
    }

    return 0;
}
