#include "parser/pci.hpp"

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
        CHECK(result.has_value());

        const auto& pci = *result;
        CHECK(pci.devices.size() == 2);

        // 设备按 map 顺序（字典序），0000:41:00.0 在前
        CHECK((pci.devices[0].address == PciAddress{0x0000, 0x41, 0x00, 0x0}));
        CHECK(pci.devices[0].vendor == Vendor{"0x10de"});
        CHECK(pci.devices[0].device_name == DeviceName{"0x2322"});
        CHECK(pci.devices[0].device_class == PciClass{"0x030000"});
        CHECK(pci.devices[0].numa_node.has_value());
        CHECK(pci.devices[0].numa_node == NumaNodeId{0});

        CHECK((pci.devices[1].address == PciAddress{0x0000, 0x65, 0x00, 0x0}));
        CHECK(pci.devices[1].vendor == Vendor{"0x15b3"});
        CHECK(pci.devices[1].device_name == DeviceName{"0x158b"});
        CHECK(pci.devices[1].device_class == PciClass{"0x020000"});
        CHECK(pci.devices[1].numa_node.has_value());
        CHECK(pci.devices[1].numa_node == NumaNodeId{1});
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
        CHECK(result.has_value());
        CHECK(result->devices.size() == 1);
        CHECK(!result->devices[0].numa_node.has_value());
    }

    // ---- 测试 3: 无 SysfsPci 数据 → nullopt ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_pci(raw, warnings);
        CHECK(!result.has_value());
    }

    // ---- 测试 4: 无效 PCI 地址 → 警告 ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/INVALID/vendor", "0x1234"));

        std::vector<std::string> warnings;
        auto result = parse_pci(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->devices.empty());
        CHECK(!warnings.empty());
    }

    // ---- 测试 5: lspci 名称覆盖 sysfs device hex ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:41:00.0/vendor", "0x10de"));
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:41:00.0/device", "0x1b06"));
        raw.records.push_back(make_record(RawSource::SysfsPci,
                                          "/sys/bus/pci/devices/0000:41:00.0/class", "0x030000"));
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:41:00.0/numa_node", "0"));

        raw.records.push_back(make_record(RawSource::Lspci, "lspci -nn",
                                          "41:00.0 VGA compatible controller [0300]: NVIDIA "
                                          "Corporation GP102 [GeForce GTX 1080 Ti] [10de:1b06]\n"));

        std::vector<std::string> warnings;
        auto result = parse_pci(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->devices.size() == 1);
        CHECK(result->devices[0].device_name ==
               DeviceName{"NVIDIA Corporation GP102 [GeForce GTX 1080 Ti]"});
        CHECK(result->devices[0].vendor == Vendor{"0x10de"});
        CHECK(result->devices[0].device_class == PciClass{"0x030000"});
    }

    // ---- 测试 6: lspci 名称含方括号且带 rev ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:00:00.0/vendor", "0x8086"));
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:00:00.0/device", "0x09a2"));
        raw.records.push_back(make_record(RawSource::SysfsPci,
                                          "/sys/bus/pci/devices/0000:00:00.0/class", "0x088000"));

        raw.records.push_back(make_record(RawSource::Lspci, "lspci -nn",
                                          "00:00.0 System peripheral [0880]: Intel Corporation Ice "
                                          "Lake Memory Map/VT-d [8086:09a2] (rev 04)\n"));

        std::vector<std::string> warnings;
        auto result = parse_pci(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->devices.size() == 1);
        CHECK(result->devices[0].device_name ==
               DeviceName{"Intel Corporation Ice Lake Memory Map/VT-d"});
        CHECK(result->devices[0].vendor == Vendor{"0x8086"});
    }

    // ---- 测试 7: lspci 独有设备（无 sysfs）被加入列表 ----
    {
        RawStore raw;
        // sysfs 仅有 41:00.0
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:41:00.0/vendor", "0x10de"));
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:41:00.0/device", "0x1b06"));
        raw.records.push_back(make_record(RawSource::SysfsPci,
                                          "/sys/bus/pci/devices/0000:41:00.0/class", "0x030000"));

        // lspci 含 41:00.0 与 65:00.0（后者无 sysfs）
        raw.records.push_back(
            make_record(RawSource::Lspci, "lspci -nn",
                        "41:00.0 VGA compatible controller [0300]: NVIDIA Corporation GP102 "
                        "[GeForce GTX 1080 Ti] [10de:1b06]\n"
                        "65:00.0 Network controller [0200]: Mellanox ConnectX-5 [15b3:158b]\n"));

        std::vector<std::string> warnings;
        auto result = parse_pci(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->devices.size() == 2);

        // 41:00.0 来自 sysfs，名称被 lspci 覆盖
        CHECK((result->devices[0].address == PciAddress{0x0000, 0x41, 0x00, 0x0}));
        CHECK(result->devices[0].device_name ==
               DeviceName{"NVIDIA Corporation GP102 [GeForce GTX 1080 Ti]"});
        CHECK(result->devices[0].vendor == Vendor{"0x10de"});

        // 65:00.0 仅来自 lspci
        CHECK((result->devices[1].address == PciAddress{0x0000, 0x65, 0x00, 0x0}));
        CHECK(result->devices[1].device_name == DeviceName{"Mellanox ConnectX-5"});
    }

    // ---- 测试 8: lspci 带域名 DDDD:BB:DD.F 格式 ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:41:00.0/vendor", "0x10de"));
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:41:00.0/device", "0x1b06"));
        raw.records.push_back(make_record(RawSource::SysfsPci,
                                          "/sys/bus/pci/devices/0000:41:00.0/class", "0x030000"));

        raw.records.push_back(make_record(RawSource::Lspci, "lspci -nn",
                                          "0000:41:00.0 VGA compatible controller [0300]: NVIDIA "
                                          "Corporation GP102 [GeForce GTX 1080 Ti] [10de:1b06]\n"));

        std::vector<std::string> warnings;
        auto result = parse_pci(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->devices.size() == 1);
        CHECK(result->devices[0].device_name ==
               DeviceName{"NVIDIA Corporation GP102 [GeForce GTX 1080 Ti]"});
    }

    TEST_SUMMARY();
}
