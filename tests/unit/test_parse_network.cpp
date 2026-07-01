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
static RawRecord make_record(RawSource source, const std::string& path, const std::string& payload)
{
    return RawRecord{source, path, payload, CollectStatus::Success,
                     std::chrono::system_clock::now()};
}

int main()
{
    // ---- 测试 1: 两个网络接口 ----
    {
        RawStore raw;
        // eth0: 有 MAC、up、speed=10000 Mbps
        raw.records.push_back(
            make_record(RawSource::SysfsNet, "/sys/class/net/eth0/address", "aa:bb:cc:dd:ee:ff"));
        raw.records.push_back(
            make_record(RawSource::SysfsNet, "/sys/class/net/eth0/operstate", "up"));
        raw.records.push_back(
            make_record(RawSource::SysfsNet, "/sys/class/net/eth0/speed", "10000"));

        // lo: 无 MAC、unknown、无 speed
        raw.records.push_back(
            make_record(RawSource::SysfsNet, "/sys/class/net/lo/operstate", "unknown"));

        std::vector<std::string> warnings;
        auto result = parse_network(raw, warnings);
        CHECK(result.has_value());

        const auto& net = *result;
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
        raw.records.push_back(
            make_record(RawSource::SysfsNet, "/sys/class/net/eth1/address", "11:22:33:44:55:66"));
        raw.records.push_back(
            make_record(RawSource::SysfsNet, "/sys/class/net/eth1/operstate", "down"));
        raw.records.push_back(
            make_record(RawSource::SysfsNet, "/sys/class/net/eth1/speed", "1000"));

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
        raw.records.push_back(
            make_record(RawSource::SysfsNet, "/sys/class/net/eth2/address", "aa:bb:cc:dd:ee:ff"));
        raw.records.push_back(
            make_record(RawSource::SysfsNet, "/sys/class/net/eth2/operstate", "up"));

        std::vector<std::string> warnings;
        auto result = parse_network(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->interfaces.size() == 1);
        CHECK(!result->interfaces[0].speed.has_value());
    }

    TEST_SUMMARY();
}
