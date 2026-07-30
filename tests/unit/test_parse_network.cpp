#include "parser/network.hpp"

#include "sysal/model/raw_store.hpp"
#include "sysal/types/enums.hpp"

#include "test_macros.hpp"
#include <chrono>
#include <string>
#include <vector>

using namespace sysal;
using namespace sysal::detail;

/// @brief 创建一条 RawRecord
static RawRecord make_record(RawSource source, const std::string &path, const std::string &payload)
{
    return RawRecord{source, path, payload, CollectStatus::Success, std::chrono::system_clock::now()};
}

int main()
{
    // ---- 测试 1: 两个网络接口 ----
    {
        RawStore raw;
        // eth0: 有 MAC、up、speed=10000 Mbps
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth0/address", "aa:bb:cc:dd:ee:ff"));
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth0/operstate", "up"));
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth0/speed", "10000"));

        // lo: 无 MAC、unknown、无 speed
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/lo/operstate", "unknown"));

        std::vector<std::string> warnings;
        auto result = parse_network(raw, warnings);
        CHECK(result.has_value());

        const auto &net = *result;
        CHECK(net.interfaces.size() == 2);

        // 按 map 字典序，eth0 在前
        CHECK(net.interfaces[0].name == InterfaceName{"eth0"});
        CHECK(net.interfaces[0].mac == MacAddress{"aa:bb:cc:dd:ee:ff"});
        CHECK(net.interfaces[0].state == InterfaceState::Up);
        CHECK(net.interfaces[0].speed.has_value());
        CHECK(net.interfaces[0].speed->value == 10000ULL * 1'000'000);
        CHECK(net.interfaces[0].visible_to_current_process == true);

        CHECK(net.interfaces[1].name == InterfaceName{"lo"});
        CHECK(net.interfaces[1].state == InterfaceState::Unknown);
        CHECK(!net.interfaces[1].speed.has_value());
        CHECK(net.interfaces[1].visible_to_current_process == true);
    }

    // ---- 测试 2: operstate=down ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth1/address", "11:22:33:44:55:66"));
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth1/operstate", "down"));
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth1/speed", "1000"));

        std::vector<std::string> warnings;
        auto result = parse_network(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->interfaces.size() == 1);
        CHECK(result->interfaces[0].state == InterfaceState::Down);
        CHECK(result->interfaces[0].speed.has_value());
        CHECK(result->interfaces[0].speed->value == 1000ULL * 1'000'000);
    }

    // ---- 测试 3: 无 SysfsNet 数据 → nullopt ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_network(raw, warnings);
        CHECK(!result.has_value());
    }

    // ---- 测试 4: 缺少 speed → nullopt ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth2/address", "aa:bb:cc:dd:ee:ff"));
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth2/operstate", "up"));

        std::vector<std::string> warnings;
        auto result = parse_network(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->interfaces.size() == 1);
        CHECK(!result->interfaces[0].speed.has_value());
    }

    // ---- 测试 5: IfAddrs 含 IPv4 与 IPv6 → addresses 填充 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth0/address", "aa:bb:cc:dd:ee:ff"));
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth0/operstate", "up"));
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/lo/operstate", "unknown"));

        std::string ifaddrs_payload = "eth0 192.168.1.10\n"
                                      "eth0 fe80::aabb:ccff:fedd:eeff\n"
                                      "lo 127.0.0.1\n"
                                      "lo ::1\n";
        raw.records.push_back(make_record(RawSource::IfAddrs, "getifaddrs", ifaddrs_payload));

        std::vector<std::string> warnings;
        auto result = parse_network(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->interfaces.size() == 2);

        // eth0 在前（字典序）
        CHECK(result->interfaces[0].name == InterfaceName{"eth0"});
        CHECK(result->interfaces[0].addresses.size() == 2);
        CHECK(result->interfaces[0].addresses[0] == IpAddress{"192.168.1.10"});
        CHECK(result->interfaces[0].addresses[1] == IpAddress{"fe80::aabb:ccff:fedd:eeff"});

        CHECK(result->interfaces[1].name == InterfaceName{"lo"});
        CHECK(result->interfaces[1].addresses.size() == 2);
        CHECK(result->interfaces[1].addresses[0] == IpAddress{"127.0.0.1"});
        CHECK(result->interfaces[1].addresses[1] == IpAddress{"::1"});
    }

    // ---- 测试 6: device 符号链接 → pci_address 填充 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth0/address", "aa:bb:cc:dd:ee:ff"));
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth0/operstate", "up"));
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth0/device", "../../../0000:41:00.0"));

        std::vector<std::string> warnings;
        auto result = parse_network(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->interfaces.size() == 1);
        CHECK(result->interfaces[0].pci_address.has_value());
        CHECK(result->interfaces[0].pci_address->domain == 0x0000);
        CHECK(result->interfaces[0].pci_address->bus == 0x41);
        CHECK(result->interfaces[0].pci_address->device == 0x00);
        CHECK(result->interfaces[0].pci_address->function == 0x0);
    }

    // ---- 测试 7: 无 IfAddrs → addresses 为空（不崩溃） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth0/address", "aa:bb:cc:dd:ee:ff"));
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth0/operstate", "up"));

        std::vector<std::string> warnings;
        auto result = parse_network(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->interfaces.size() == 1);
        CHECK(result->interfaces[0].addresses.empty());
    }

    // ---- 测试 8: 虚拟接口无 device 符号链接 → pci_address 为 nullopt ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/lo/operstate", "unknown"));

        std::vector<std::string> warnings;
        auto result = parse_network(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->interfaces.size() == 1);
        CHECK(!result->interfaces[0].pci_address.has_value());
    }

    // ---- 测试 9: IfAddrs 畸形输入（无空格、空行）不崩溃且产生警告 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth0/address", "aa:bb:cc:dd:ee:ff"));
        raw.records.push_back(make_record(RawSource::SysfsNet, "/sys/class/net/eth0/operstate", "up"));

        std::string malformed = "eth0 192.168.1.10\n"
                                "\n"
                                "no_space_line\n"
                                "   \n"
                                "eth0 10.0.0.1\n";
        raw.records.push_back(make_record(RawSource::IfAddrs, "getifaddrs", malformed));

        std::vector<std::string> warnings;
        auto result = parse_network(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->interfaces.size() == 1);
        CHECK(result->interfaces[0].addresses.size() == 2);
        CHECK(result->interfaces[0].addresses[0] == IpAddress{"192.168.1.10"});
        CHECK(result->interfaces[0].addresses[1] == IpAddress{"10.0.0.1"});
        bool has_warning = false;
        for(const auto &w : warnings)
        {
            if(w.find("IfAddrs") != std::string::npos)
            {
                has_warning = true;
                break;
            }
        }
        CHECK(has_warning);
    }

    TEST_SUMMARY();
}
