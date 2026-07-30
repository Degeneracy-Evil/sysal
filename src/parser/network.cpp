/// @file network.cpp
/// @brief 网络解析器实现
/// @details 从 sysfs 网络接口数据解析网络设备信息。

#include "network.hpp"

#include "parse_utils.hpp"

#include <map>
#include <string_view>
#include <vector>

namespace sysal::detail
{

    namespace
    {

        /// @brief 从 sysfs 路径提取接口名
        /// @param path sysfs 路径，如 "/sys/class/net/eth0/address"
        /// @return 接口名，如 "eth0"
        std::string extract_interface_name_from_path(std::string_view path)
        {
            // 路径格式: /sys/class/net/<ifname>/<file>
            // 或 /sys/class/net/<ifname> (目录本身)
            const std::string_view prefix = "/sys/class/net/";
            auto pos = path.find(prefix);
            if(pos == std::string_view::npos)
            {
                return {};
            }
            auto after_prefix = path.substr(pos + prefix.size());
            auto slash = after_prefix.find('/');
            if(slash == std::string_view::npos)
            {
                return std::string(after_prefix);
            }
            return std::string(after_prefix.substr(0, slash));
        }

        /// @brief 解析链路状态字符串
        /// @param state_str operstate 文件内容
        /// @return InterfaceState 枚举值
        InterfaceState parse_interface_state(std::string_view state_str)
        {
            auto trimmed = trim(state_str);
            if(trimmed == "up")
            {
                return InterfaceState::Up;
            }
            if(trimmed == "down")
            {
                return InterfaceState::Down;
            }
            return InterfaceState::Unknown;
        }

    } // namespace

    std::optional<Network> parse_network(const RawStore &raw, std::vector<std::string> &warnings)
    {
        auto net_records = raw.get_all(RawSource::SysfsNet);
        if(net_records.empty())
        {
            warnings.push_back("parse_network: 缺少 SysfsNet 数据");
            return std::nullopt;
        }

        // 按接口名分组
        std::map<std::string, std::vector<const RawRecord *>> groups;
        for(const auto *rec : net_records)
        {
            if(rec->status != CollectStatus::Success)
            {
                continue;
            }

            auto ifname = extract_interface_name_from_path(rec->path_or_command);
            if(ifname.empty())
            {
                warnings.push_back("parse_network: 无法从路径提取接口名: " + rec->path_or_command);
                continue;
            }
            groups[ifname].push_back(rec);
        }

        // 解析 IfAddrs：payload 格式 "ifname ip\n" 每行一条
        std::map<std::string, std::vector<std::string>> ip_map;
        auto ifaddr_records = raw.get_all(RawSource::IfAddrs);
        for(const auto *rec : ifaddr_records)
        {
            if(rec->status != CollectStatus::Success || rec->payload.empty())
            {
                continue;
            }
            for(const auto &line : split(rec->payload, '\n'))
            {
                if(line.empty())
                {
                    continue;
                }
                auto space = line.find(' ');
                if(space == std::string::npos)
                {
                    warnings.push_back("parse_network: IfAddrs 行无空格分隔: " + line);
                    continue;
                }
                auto ifname = trim(line.substr(0, space));
                auto ip = trim(line.substr(space + 1));
                if(ifname.empty() || ip.empty())
                {
                    warnings.push_back("parse_network: IfAddrs 行接口名或 IP 为空: " + line);
                    continue;
                }
                ip_map[ifname].push_back(ip);
            }
        }

        Network network;
        for(const auto &[ifname, records] : groups)
        {
            NetworkInterface iface;
            iface.name = InterfaceName{ifname};
            iface.visible_to_current_process = true;

            for(const auto *rec : records)
            {
                auto filename = extract_filename(rec->path_or_command);
                const auto &payload = rec->payload;

                if(filename == "address")
                {
                    auto trimmed = trim(payload);
                    iface.mac = MacAddress{trimmed};
                }
                else if(filename == "operstate")
                {
                    iface.state = parse_interface_state(payload);
                }
                else if(filename == "speed")
                {
                    auto trimmed = trim(payload);
                    auto val = parse_uint(trimmed);
                    if(val.has_value() && *val > 0)
                    {
                        // speed 文件单位为 Mbps，转换为 bps
                        iface.speed = Bandwidth{*val * 1'000'000};
                    }
                    else
                    {
                        // speed 不可用（如回环接口），保持 nullopt
                    }
                }
                else if(filename == "device")
                {
                    // payload 是符号链接目标，如 "../../../0000:41:00.0"
                    auto last_slash = payload.rfind('/');
                    auto pci_str =
                        last_slash == std::string::npos ? trim(payload) : trim(payload.substr(last_slash + 1));
                    auto pci = parse_pci_address(pci_str);
                    if(pci.has_value())
                    {
                        iface.pci_address = pci;
                    }
                    else if(!pci_str.empty())
                    {
                        warnings.push_back("parse_network: PCI 地址解析失败: " + pci_str);
                    }
                }
            }

            // 填充 IP 地址
            auto ip_it = ip_map.find(ifname);
            if(ip_it != ip_map.end())
            {
                for(const auto &ip : ip_it->second)
                {
                    iface.addresses.push_back(IpAddress{ip});
                }
            }

            network.interfaces.push_back(iface);
        }

        return network;
    }

} // namespace sysal::detail
