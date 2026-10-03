#include "parser/network_details.hpp"
#include "parser/parse_utils.hpp"

#include <algorithm>
#include <sstream>
#include <string_view>

namespace sysal::detail
{
    namespace
    {
        void parse_ethtool(NetworkInterface &iface, std::string_view payload)
        {
            std::vector<std::string> *modes = nullptr;
            for(const auto &line : split(payload, '\n'))
            {
                const auto colon = line.find(':');
                std::string value;
                if(colon == std::string::npos)
                    value = trim(line);
                else
                {
                    auto [key, entry] = parse_kv(line, ':');
                    value = entry;
                    modes = key == "Supported link modes"                 ? &iface.supported_link_modes
                            : key == "Advertised link modes"              ? &iface.advertised_link_modes
                            : key == "Link partner advertised link modes" ? &iface.peer_link_modes
                                                                          : nullptr;
                    if(key == "firmware-version")
                        iface.firmware_version = hardware_text(value);
                    else if(key == "version")
                        iface.driver_version = hardware_text(value);
                    else if(key == "Auto-negotiation" && (value == "on" || value == "off"))
                        iface.autonegotiation = value == "on";
                    else if(key == "Permanent address" && !value.empty())
                    {
                        // Never substitute a placeholder permanent address for the current address.
                        const bool nonzero =
                            std::any_of(value.begin(), value.end(), [](char ch) { return ch != '0' && ch != ':'; });
                        if(nonzero && value.find(':') != std::string::npos)
                            iface.permanent_mac = MacAddress{value};
                    }
                }
                if(modes)
                {
                    std::istringstream tokens(value);
                    std::string mode;
                    while(tokens >> mode)
                        if(mode.find('/') != std::string::npos &&
                           std::find(modes->begin(), modes->end(), mode) == modes->end())
                            modes->push_back(mode);
                }
            }
        }
    } // namespace

    void apply_network_details(Network &network, const RawStore &raw)
    {
        constexpr std::string_view prefix = "ethtool/";
        for(const auto *record : raw.get_all(RawSource::Ethtool))
        {
            if((record->status != CollectStatus::Success && record->status != CollectStatus::Partial) ||
               !record->path_or_command.starts_with(prefix))
                continue;
            const auto slash = record->path_or_command.find('/', prefix.size());
            const auto name = record->path_or_command.substr(prefix.size(), slash - prefix.size());
            const auto iface = std::find_if(network.interfaces.begin(), network.interfaces.end(),
                                            [&](const auto &item) { return item.name.value == name; });
            if(iface != network.interfaces.end())
                parse_ethtool(*iface, record->payload);
        }
        for(const auto *record : raw.get_all(RawSource::ProcNetVlan))
        {
            if(record->status != CollectStatus::Success)
                continue;
            for(const auto &line : split(record->payload, '\n'))
            {
                const auto fields = split(line, '|');
                if(fields.size() != 3)
                    continue;
                const auto id = parse_uint(fields[1]);
                if(!id || *id > 4095)
                    continue;
                const auto name = trim(fields[0]);
                const auto parent = trim(fields[2]);
                const auto iface = std::find_if(network.interfaces.begin(), network.interfaces.end(),
                                                [&](const auto &item) { return item.name.value == name; });
                if(iface == network.interfaces.end() || parent.empty())
                    continue;
                iface->interface_kind = "vlan";
                iface->vlan_id = static_cast<std::uint32_t>(*id);
                iface->vlan_parent = InterfaceName{parent};
            }
        }
    }
} // namespace sysal::detail
