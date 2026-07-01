/// @file parse_utils.cpp
/// @brief 解析工具函数实现
/// @details 提供字符串分割、trim、数值解析等通用工具。

#include "parse_utils.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <utility>

namespace sysal::detail
{

std::string trim(std::string_view s)
{
    auto begin = s.begin();
    while(begin != s.end() && std::isspace(static_cast<unsigned char>(*begin)))
    {
        ++begin;
    }
    auto end = s.end();
    while(end != begin && std::isspace(static_cast<unsigned char>(*(end - 1))))
    {
        --end;
    }
    return std::string(begin, end);
}

std::vector<std::string> split(std::string_view s, char delimiter)
{
    std::vector<std::string> result;
    std::string current;
    for(auto ch : s)
    {
        if(ch == delimiter)
        {
            result.push_back(std::move(current));
            current.clear();
        }
        else
        {
            current += ch;
        }
    }
    result.push_back(std::move(current));
    return result;
}

std::pair<std::string, std::string> parse_kv(std::string_view line, char separator)
{
    auto pos = line.find(separator);
    if(pos == std::string_view::npos)
    {
        return {trim(line), {}};
    }
    return {trim(line.substr(0, pos)), trim(line.substr(pos + 1))};
}

std::optional<std::uint64_t> parse_uint(std::string_view s)
{
    auto trimmed = trim(s);
    if(trimmed.empty())
    {
        return std::nullopt;
    }
    std::uint64_t value{};
    auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value, 10);
    if(ec != std::errc{})
    {
        return std::nullopt;
    }
    if(ptr != trimmed.data() + trimmed.size())
    {
        return std::nullopt; // 部分消费：剩余字符非数字
    }
    return value;
}

std::optional<std::uint64_t> parse_hex(std::string_view s)
{
    auto trimmed = trim(s);
    if(trimmed.empty())
    {
        return std::nullopt;
    }
    std::uint64_t value{};
    auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value, 16);
    if(ec != std::errc{})
    {
        return std::nullopt;
    }
    if(ptr != trimmed.data() + trimmed.size())
    {
        return std::nullopt; // 部分消费：剩余字符非十六进制
    }
    return value;
}

std::optional<PciAddress> parse_pci_address(std::string_view s)
{
    auto trimmed = trim(s);
    if(trimmed.empty())
    {
        return std::nullopt;
    }

    auto dot_pos = trimmed.find('.');
    if(dot_pos == std::string_view::npos)
    {
        return std::nullopt;
    }

    auto func_str = trimmed.substr(dot_pos + 1);
    auto func = parse_hex(func_str);
    if(!func.has_value() || *func > 255)
    {
        return std::nullopt;
    }

    auto before_dot = trimmed.substr(0, dot_pos);
    auto last_colon = before_dot.rfind(':');
    if(last_colon == std::string_view::npos)
    {
        return std::nullopt;
    }

    auto device_str = before_dot.substr(last_colon + 1);
    auto device = parse_hex(device_str);
    if(!device.has_value() || *device > 255)
    {
        return std::nullopt;
    }

    auto before_device = before_dot.substr(0, last_colon);
    auto second_colon = before_device.rfind(':');
    if(second_colon == std::string_view::npos)
    {
        return std::nullopt;
    }

    auto bus_str = before_device.substr(second_colon + 1);
    auto bus = parse_hex(bus_str);
    if(!bus.has_value() || *bus > 255)
    {
        return std::nullopt;
    }

    auto domain_str = before_device.substr(0, second_colon);
    auto domain = parse_hex(domain_str);
    if(!domain.has_value() || *domain > 65535)
    {
        return std::nullopt;
    }

    return PciAddress{static_cast<std::uint16_t>(*domain), static_cast<std::uint8_t>(*bus),
                      static_cast<std::uint8_t>(*device), static_cast<std::uint8_t>(*func)};
}

std::optional<MemorySize> parse_kb_to_bytes(std::string_view s)
{
    auto value = parse_uint(s);
    if(!value.has_value())
    {
        return std::nullopt;
    }
    return MemorySize{*value * 1024};
}

} // namespace sysal::detail
