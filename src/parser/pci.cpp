/// @file pci.cpp
/// @brief PCI 解析器实现
/// @details 从 sysfs 和 lspci 输出解析 PCI 设备信息。

#include "pci.hpp"

#include "parse_utils.hpp"

#include <map>
#include <optional>
#include <string_view>
#include <utility>

namespace sysal::detail
{

namespace
{

/// @brief 从 sysfs 路径提取 PCI 地址
/// @param path sysfs 路径，如 "/sys/bus/pci/devices/0000:41:00.0/vendor"
/// @return PCI 地址字符串（路径最后一段），如 "0000:41:00.0"
std::string extract_pci_address_from_path(std::string_view path)
{
    // 路径格式: /sys/bus/pci/devices/DDDDD:BB:DD.F/<file>
    // 或 /sys/bus/pci/devices/DDDDD:BB:DD.F (目录本身)
    // 需要提取 DDDDD:BB:DD.F 部分
    auto last_slash = path.rfind('/');
    if(last_slash == std::string_view::npos)
    {
        return std::string(path);
    }
    auto after_slash = path.substr(last_slash + 1);

    // 如果 after_slash 是文件名（vendor, device, class, numa_node），
    // 则需要再往前取一段
    if(after_slash == "vendor" || after_slash == "device" || after_slash == "class" ||
       after_slash == "numa_node")
    {
        auto rest = path.substr(0, last_slash);
        auto prev_slash = rest.rfind('/');
        if(prev_slash == std::string_view::npos)
        {
            return std::string(rest);
        }
        return std::string(rest.substr(prev_slash + 1));
    }

    return std::string(after_slash);
}

/// @brief 归一化 lspci 地址为 DDDD:BB:DD.F 格式
/// @param addr_raw lspci 输出中的地址字段（BB:DD.F 或 DDDD:BB:DD.F）
/// @return 归一化后的地址字符串
std::string normalize_lspci_address(std::string_view addr_raw)
{
    auto first_colon = addr_raw.find(':');
    auto second_colon = (first_colon != std::string_view::npos)
                            ? addr_raw.find(':', first_colon + 1)
                            : std::string_view::npos;
    if(second_colon == std::string_view::npos)
    {
        return "0000:" + std::string(addr_raw);
    }
    return std::string(addr_raw);
}

/// @brief 解析单行 lspci -nn 输出
/// @param line lspci 输出一行
/// @return 成功返回 (归一化地址, 设备名)，失败返回 nullopt
/// @details 行格式: DD:DD.F Class_name [class_hex]: DeviceName [vendor:device] (rev NN)
///          设备名可能包含方括号（如 [GeForce GTX 1080 Ti]），故取最后一个
///          含 vendor:device 十六进制对的方括号之前的内容作为设备名。
std::optional<std::pair<std::string, std::string>> parse_lspci_line(std::string_view line)
{
    auto trimmed = trim(line);
    if(trimmed.empty())
    {
        return std::nullopt;
    }

    auto space_pos = trimmed.find(' ');
    if(space_pos == std::string_view::npos)
    {
        return std::nullopt;
    }
    auto addr_raw = trimmed.substr(0, space_pos);
    auto addr_str = normalize_lspci_address(addr_raw);
    if(!parse_pci_address(addr_str).has_value())
    {
        return std::nullopt;
    }

    auto rest = trimmed.substr(space_pos + 1);

    auto sep = rest.find("]: ");
    if(sep == std::string_view::npos)
    {
        return std::nullopt;
    }
    auto tail = rest.substr(sep + 3);

    auto last_bracket = tail.rfind('[');
    if(last_bracket == std::string_view::npos)
    {
        return std::nullopt;
    }
    auto close_bracket = tail.find(']', last_bracket);
    if(close_bracket == std::string_view::npos)
    {
        return std::nullopt;
    }
    auto inner = tail.substr(last_bracket + 1, close_bracket - last_bracket - 1);
    auto colon = inner.find(':');
    if(colon == std::string_view::npos)
    {
        return std::nullopt;
    }
    if(!parse_hex(inner.substr(0, colon)).has_value() ||
       !parse_hex(inner.substr(colon + 1)).has_value())
    {
        return std::nullopt;
    }

    auto name = trim(tail.substr(0, last_bracket));
    if(name.empty())
    {
        return std::nullopt;
    }

    return std::make_pair(std::move(addr_str), std::move(name));
}

} // namespace

std::optional<Pci> parse_pci(const RawStore& raw, std::vector<std::string>& warnings)
{
    auto pci_records = raw.get_all(RawSource::SysfsPci);
    if(pci_records.empty())
    {
        warnings.push_back("parse_pci: 缺少 SysfsPci 数据");
        return std::nullopt;
    }

    // 按 PCI 地址分组
    std::map<std::string, std::vector<const RawRecord*>> groups;
    for(const auto* rec : pci_records)
    {
        if(rec->status != CollectStatus::Success)
        {
            continue;
        }

        auto addr_str = extract_pci_address_from_path(rec->path_or_command);
        groups[addr_str].push_back(rec);
    }

    Pci pci;
    // 地址 → devices 索引，用于与 lspci 合并
    std::map<std::string, std::size_t> addr_index;
    for(const auto& [addr_str, records] : groups)
    {
        PciDevice dev;

        // 解析 PCI 地址
        auto addr = parse_pci_address(addr_str);
        if(!addr.has_value())
        {
            warnings.push_back("parse_pci: 无法解析 PCI 地址: " + addr_str);
            continue;
        }
        dev.address = *addr;

        // 从分组记录中提取各字段
        for(const auto* rec : records)
        {
            auto filename = extract_filename(rec->path_or_command);
            const auto& payload = rec->payload;

            if(filename == "vendor")
            {
                // hex 字符串如 "0x10de"，保留原始值作为 NamedString
                auto trimmed = trim(payload);
                dev.vendor = Vendor{trimmed};
            }
            else if(filename == "device")
            {
                auto trimmed = trim(payload);
                dev.device_name = DeviceName{trimmed};
            }
            else if(filename == "class")
            {
                auto trimmed = trim(payload);
                dev.device_class = PciClass{trimmed};
            }
            else if(filename == "numa_node")
            {
                auto trimmed = trim(payload);
                auto val = parse_uint(trimmed);
                if(val.has_value())
                {
                    dev.numa_node = NumaNodeId{static_cast<std::uint32_t>(*val)};
                }
                else
                {
                    // parse_uint 无法解析负数，尝试直接检查 "-1"
                    if(trimmed == "-1")
                    {
                        dev.numa_node = std::nullopt;
                    }
                    else
                    {
                        warnings.push_back("parse_pci: numa_node 值解析失败: " + trimmed);
                    }
                }
            }
        }

        addr_index[addr_str] = pci.devices.size();
        pci.devices.push_back(dev);
    }

    // 解析 lspci 输出并与 sysfs 数据合并
    auto lspci_records = raw.get_all(RawSource::Lspci);
    for(const auto* rec : lspci_records)
    {
        if(rec->status != CollectStatus::Success)
        {
            continue;
        }
        auto lines = split(rec->payload, '\n');
        for(const auto& line : lines)
        {
            auto parsed = parse_lspci_line(line);
            if(!parsed.has_value())
            {
                continue;
            }
            const auto& [addr_str, name] = *parsed;
            auto it = addr_index.find(addr_str);
            if(it != addr_index.end())
            {
                pci.devices[it->second].device_name = DeviceName{name};
            }
            else
            {
                auto addr = parse_pci_address(addr_str);
                if(!addr.has_value())
                {
                    continue;
                }
                PciDevice dev;
                dev.address = *addr;
                dev.device_name = DeviceName{name};
                addr_index[addr_str] = pci.devices.size();
                pci.devices.push_back(dev);
                warnings.push_back("parse_pci: lspci 独有设备 " + addr_str + "，sysfs 数据缺失");
            }
        }
    }

    return pci;
}

} // namespace sysal::detail
