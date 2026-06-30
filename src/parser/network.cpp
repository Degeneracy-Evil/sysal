#include "network.hpp"

#include "parse_utils.hpp"

#include <map>
#include <string_view>

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

/// @brief 从 sysfs 路径提取文件名
/// @param path sysfs 路径，如 "/sys/class/net/eth0/address"
/// @return 文件名，如 "address"
std::string extract_filename_from_path(std::string_view path)
{
    auto last_slash = path.rfind('/');
    if(last_slash == std::string_view::npos)
    {
        return std::string(path);
    }
    return std::string(path.substr(last_slash + 1));
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

std::optional<Network> parse_network(const RawStore& raw, std::vector<std::string>& warnings)
{
    auto net_records = raw.get_all(RawSource::SysfsNet);
    if(net_records.empty())
    {
        warnings.push_back("parse_network: 缺少 SysfsNet 数据");
        return std::nullopt;
    }

    // 按接口名分组
    std::map<std::string, std::vector<const RawRecord*>> groups;
    for(const auto* rec : net_records)
    {
        auto ifname = extract_interface_name_from_path(rec->path_or_command);
        if(ifname.empty())
        {
            warnings.push_back("parse_network: 无法从路径提取接口名: " + rec->path_or_command);
            continue;
        }
        groups[ifname].push_back(rec);
    }

    Network network;
    for(const auto& [ifname, records] : groups)
    {
        NetworkInterface iface;
        iface.name = InterfaceName{ifname};
        iface.visible_to_current_process = true;

        for(const auto* rec : records)
        {
            auto filename = extract_filename_from_path(rec->path_or_command);
            const auto& payload = rec->payload;

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
        }

        network.interfaces.push_back(iface);
    }

    return network;
}

} // namespace sysal::detail
