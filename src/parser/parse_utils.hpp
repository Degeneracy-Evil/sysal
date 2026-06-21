#pragma once

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/value_types.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sysal::detail
{

inline std::string trim(std::string_view s)
{
    const auto start =
        std::find_if_not(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
    const auto end =
        std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c); })
            .base();
    if(start >= end)
    {
        return {};
    }
    return std::string(start, end);
}

inline std::vector<std::string> split(std::string_view s, char delim)
{
    std::vector<std::string> result;
    std::string current;
    for(char c : s)
    {
        if(c == delim)
        {
            result.push_back(current);
            current.clear();
        }
        else
        {
            current += c;
        }
    }
    result.push_back(current);
    return result;
}

inline std::pair<std::string, std::string> split_kv(std::string_view line, char delim = ':')
{
    const auto pos = line.find(delim);
    if(pos == std::string_view::npos)
    {
        return {};
    }
    return {trim(line.substr(0, pos)), trim(line.substr(pos + 1))};
}

inline std::optional<std::uint64_t> parse_uint(std::string_view s)
{
    if(s.empty())
    {
        return std::nullopt;
    }
    std::uint64_t result{};
    auto res = std::from_chars(s.data(), s.data() + s.size(), result);
    if(res.ec != std::errc{})
    {
        return std::nullopt;
    }
    return result;
}

inline std::string unquote(std::string_view s)
{
    if(s.size() >= 2 && s.front() == '"' && s.back() == '"')
    {
        return std::string(s.substr(1, s.size() - 2));
    }
    return std::string(s);
}

inline std::unordered_map<std::string, std::string> build_path_map(const RawStore& raw,
                                                                   RawSource source)
{
    std::unordered_map<std::string, std::string> map;
    for(const auto& record : raw.records)
    {
        if(record.source == source && record.status == CollectStatus::Success)
        {
            map[record.path_or_command] = record.payload;
        }
    }
    return map;
}

inline void add_warning(Diagnostics& diag, std::string message,
                        std::optional<RawSource> source = std::nullopt)
{
    diag.records.push_back({
        .severity = Severity::Warning,
        .message = std::move(message),
        .source = source,
        .conflict = std::nullopt,
    });
}

inline PciAddress parse_pci_address(std::string_view addr)
{
    PciAddress result{};
    auto parts = split(addr, ':');
    if(parts.size() >= 3)
    {
        result.domain = static_cast<std::uint16_t>(parse_uint(parts[0]).value_or(0));
        result.bus = static_cast<std::uint8_t>(parse_uint(parts[1]).value_or(0));
        auto dev_func = split(parts[2], '.');
        if(dev_func.size() >= 2)
        {
            result.device = static_cast<std::uint8_t>(parse_uint(dev_func[0]).value_or(0));
            result.function = static_cast<std::uint8_t>(parse_uint(dev_func[1]).value_or(0));
        }
    }
    return result;
}

constexpr std::string_view kNodePrefix = "/sys/devices/system/node/";

inline std::optional<std::uint64_t> extract_kb(const std::string& value)
{
    auto parts = split(value, ' ');
    if(parts.empty())
    {
        return std::nullopt;
    }
    auto kb = parse_uint(parts[0]);
    if(!kb)
    {
        return std::nullopt;
    }
    return *kb * 1024U;
}

inline std::optional<std::uint32_t> node_id_from_key(std::string_view key)
{
    if(!key.starts_with("node"))
    {
        return std::nullopt;
    }
    auto num = key.substr(4);
    auto val = parse_uint(num);
    if(!val)
    {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*val);
}

inline Architecture arch_from_machine(std::string_view machine)
{
    if(machine == "x86_64")
    {
        return Architecture::X86_64;
    }
    if(machine == "aarch64")
    {
        return Architecture::AArch64;
    }
    if(machine == "riscv64")
    {
        return Architecture::Riscv64;
    }
    return Architecture::Other;
}

inline std::vector<std::string>
extract_prefix_keys(const std::unordered_map<std::string, std::string>& path_map,
                    std::string_view prefix)
{
    std::vector<std::string> keys;
    for(const auto& [path, content] : path_map)
    {
        if(!path.starts_with(prefix))
        {
            continue;
        }
        auto rest = std::string_view(path).substr(prefix.size());
        auto slash = rest.find('/');
        if(slash == std::string_view::npos)
        {
            continue;
        }
        auto key = std::string(rest.substr(0, slash));
        if(std::find(keys.begin(), keys.end(), key) == keys.end())
        {
            keys.push_back(key);
        }
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

} // namespace sysal::detail
