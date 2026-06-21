#include "network_parser.hpp"
#include "parse_utils.hpp"
#include "parsed_facts.hpp"

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>

#include <array>
#include <cstring>
#include <string>
#include <unordered_map>

namespace sysal::detail
{

namespace
{

std::unordered_map<std::string, std::vector<IpAddress>> collect_all_addresses()
{
    std::unordered_map<std::string, std::vector<IpAddress>> result;

    struct ifaddrs* ifa_head = nullptr;
    if(getifaddrs(&ifa_head) != 0)
    {
        return result;
    }

    for(struct ifaddrs* ifa = ifa_head; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if(ifa->ifa_addr == nullptr)
        {
            continue;
        }

        const auto family = ifa->ifa_addr->sa_family;
        if(family != AF_INET && family != AF_INET6)
        {
            continue;
        }

        std::array<char, INET6_ADDRSTRLEN> buffer{};
        const void* src = nullptr;
        if(family == AF_INET)
        {
            src = static_cast<const void*>(reinterpret_cast<sockaddr_in*>(ifa->ifa_addr));
        }
        else
        {
            src = static_cast<const void*>(reinterpret_cast<sockaddr_in6*>(ifa->ifa_addr));
        }

        if(inet_ntop(family, src, buffer.data(), buffer.size()) != nullptr)
        {
            result[ifa->ifa_name].push_back(IpAddress{std::string(buffer.data())});
        }
    }

    freeifaddrs(ifa_head);
    return result;
}

} // namespace

std::optional<NetworkSubsystem> parse_network(const RawStore& raw, Diagnostics& diag)
{
    auto records = raw.get_all(RawSource::SysfsNet);
    if(records.empty())
    {
        return std::nullopt;
    }

    auto path_map = build_path_map(raw, RawSource::SysfsNet);
    auto interface_names = extract_prefix_keys(path_map, "/sys/class/net/");
    if(interface_names.empty())
    {
        return std::nullopt;
    }

    auto address_map = collect_all_addresses();

    NetworkSubsystem facts;
    for(const auto& iface : interface_names)
    {
        NetworkInterface net_iface;
        net_iface.name = InterfaceName{iface};

        const auto base = "/sys/class/net/" + iface + "/";

        auto operstate_it = path_map.find(base + "operstate");
        if(operstate_it != path_map.end())
        {
            auto state = trim(operstate_it->second);
            if(state == "up")
            {
                net_iface.state = InterfaceState::Up;
            }
            else if(state == "down")
            {
                net_iface.state = InterfaceState::Down;
            }
            else
            {
                net_iface.state = InterfaceState::Unknown;
            }
        }

        auto address_it = path_map.find(base + "address");
        if(address_it != path_map.end())
        {
            net_iface.mac = MacAddress{trim(address_it->second)};
        }

        auto speed_it = path_map.find(base + "speed");
        if(speed_it != path_map.end())
        {
            auto speed = parse_uint(trim(speed_it->second));
            if(speed)
            {
                net_iface.speed = Bandwidth{*speed * 1000000U};
            }
        }

        auto device_it = path_map.find(base + "device");
        if(device_it != path_map.end())
        {
            auto pci_str = trim(device_it->second);
            if(!pci_str.empty())
            {
                net_iface.pci_address = parse_pci_address(pci_str);
            }
        }

        auto addr_it = address_map.find(iface);
        if(addr_it != address_map.end())
        {
            net_iface.addresses = addr_it->second;
        }

        facts.interfaces.push_back(net_iface);
    }

    if(facts.interfaces.empty())
    {
        add_warning(diag, "network parser found no interfaces", RawSource::SysfsNet);
    }

    return facts;
}

} // namespace sysal::detail
