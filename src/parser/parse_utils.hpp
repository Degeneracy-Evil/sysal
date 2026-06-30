/// @file parse_utils.hpp
/// @brief 解析器共享的工具函数集合
/// @details 提供字符串裁剪、分割、键值对解析、无符号整数解析、
///          KB 单位换算、PCI 地址解析等基础工具，以及从 RawStore
///          构建"路径 -> 内容"映射、添加诊断告警等辅助函数。
///          这些工具被 storage / software / accelerator / topology /
///          execution 等解析器复用，避免重复实现。

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

/// @brief 裁剪字符串两端的空白字符
/// @param s 待裁剪的字符串视图
/// @return 裁剪后的字符串；若字符串全为空白则返回空串
/// @details 使用 `std::isspace` 判断空白，lambda 中将字符强制转换为
///          `unsigned char`，避免传入 signed char 时出现未定义行为。
inline std::string trim(std::string_view s)
{
    // 从左向右找到第一个非空白字符
    const auto start =
        std::find_if_not(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
    // 从右向左找到第一个非空白字符，再取其后的迭代器作为结束位置
    const auto end =
        std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c); })
            .base();
    if(start >= end)
    {
        // 整段为空白或为空
        return {};
    }
    return std::string(start, end);
}

/// @brief 按分隔符切分字符串
/// @param s 待切分的字符串视图
/// @param delim 分隔符字符
/// @return 切分后的子串列表，保留空段（与 csv 等格式一致）
/// @details 不对每段做裁剪，调用方需要时自行调用 trim。
///          末尾会追加最后一段，保证结果至少包含一个元素。
inline std::vector<std::string> split(std::string_view s, char delim)
{
    std::vector<std::string> result;
    std::string current;
    for(char c : s)
    {
        if(c == delim)
        {
            // 遇到分隔符：保存当前段并开始新的一段
            result.push_back(current);
            current.clear();
        }
        else
        {
            current += c;
        }
    }
    // 追加末尾段
    result.push_back(current);
    return result;
}

/// @brief 将一行切分为"键: 值"形式的键值对
/// @param line 待解析的行
/// @param delim 键值分隔符，默认为冒号
/// @return 由裁剪后的键和值组成的 pair；若找不到分隔符则返回空 pair
/// @details 键与值两侧的空白都会被 trim 掉，便于后续比较。
inline std::pair<std::string, std::string> split_kv(std::string_view line, char delim = ':')
{
    const auto pos = line.find(delim);
    if(pos == std::string_view::npos)
    {
        // 没有分隔符，无法构成键值对
        return {};
    }
    return {trim(line.substr(0, pos)), trim(line.substr(pos + 1))};
}

/// @brief 将字符串解析为无符号 64 位整数
/// @param s 待解析的字符串视图
/// @return 解析成功返回数值；空串或解析失败返回 std::nullopt
/// @details 使用 `std::from_chars` 进行零拷贝快速解析，不接受前导空白、
///          正负号或前缀。仅识别十进制整数。
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
        // 解析失败（如包含非数字字符或溢出）
        return std::nullopt;
    }
    return result;
}

/// @brief 去除字符串首尾的双引号
/// @param s 待处理的字符串视图
/// @return 若 s 首尾均为双引号且长度大于等于 2，则返回去掉引号后的内容；否则原样返回
inline std::string unquote(std::string_view s)
{
    if(s.size() >= 2 && s.front() == '"' && s.back() == '"')
    {
        return std::string(s.substr(1, s.size() - 2));
    }
    return std::string(s);
}

/// @brief 从 RawStore 中构建"路径 -> 内容"映射
/// @param raw 原始数据存储
/// @param source 要筛选的数据来源
/// @return 仅包含指定来源且采集成功记录的映射，键为 path_or_command，值为 payload
/// @details 采集失败（status != Success）的记录会被跳过，避免脏数据进入解析流程。
inline std::unordered_map<std::string, std::string> build_path_map(const RawStore& raw,
                                                                   RawSource source)
{
    std::unordered_map<std::string, std::string> map;
    for(const auto& record : raw.records)
    {
        // 仅收录采集成功的目标来源记录
        if(record.source == source && record.status == CollectStatus::Success)
        {
            map[record.path_or_command] = record.payload;
        }
    }
    return map;
}

/// @brief 向 Diagnostics 追加一条告警记录
/// @param diag 诊断信息容器
/// @param message 告警消息内容
/// @param source 可选的原始数据来源，用于追溯告警出处
/// @details severity 固定为 Warning，conflict 字段留空。
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

/// @brief 解析 PCI 地址字符串为 PciAddress 结构
/// @param addr 形如 "domain:bus:device.function" 的地址字符串
/// @return 填充后的 PciAddress；格式不符时各字段保持为 0
/// @details 期望格式为十六进制四段：`DDDD:BB:DD.F`，其中
///          - domain: 16 位
///          - bus: 8 位
///          - device: 5 位（这里存为 8 位）
///          - function: 3 位（这里存为 8 位）
///          字段缺失或解析失败时对应字段回退为 0。
inline PciAddress parse_pci_address(std::string_view addr)
{
    PciAddress result{};
    // 先按冒号切出 domain / bus / device.function 三段
    auto parts = split(addr, ':');
    if(parts.size() >= 3)
    {
        result.domain = static_cast<std::uint16_t>(parse_uint(parts[0]).value_or(0));
        result.bus = static_cast<std::uint8_t>(parse_uint(parts[1]).value_or(0));
        // 第三段再按点号切出 device 与 function
        auto dev_func = split(parts[2], '.');
        if(dev_func.size() >= 2)
        {
            result.device = static_cast<std::uint8_t>(parse_uint(dev_func[0]).value_or(0));
            result.function = static_cast<std::uint8_t>(parse_uint(dev_func[1]).value_or(0));
        }
    }
    return result;
}

/// @brief sysfs NUMA 节点目录前缀
/// @details 形如 /sys/devices/system/node/node0/、node1/ 等。
constexpr std::string_view kNodePrefix = "/sys/devices/system/node/";

/// @brief 从形如 "12345 kB" 的字符串中提取字节数
/// @param value 包含数字和单位的字符串，第一个以空格分隔的字段为 KB 数值
/// @return 转换后的字节数（KB * 1024）；无法解析时返回 std::nullopt
/// @details 典型输入来自 sysfs meminfo，例如 "MemTotal:  16384000 kB"。
///          仅取首段数字，忽略后续的单位标识。
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
    // 将 KB 换算为字节
    return *kb * 1024U;
}

/// @brief 从形如 "node0" 的键名中提取 NUMA 节点编号
/// @param key 以 "node" 为前缀、后接数字编号的键名
/// @return 节点编号；前缀不符或后缀非数字时返回 std::nullopt
/// @details 去除前缀 "node"（4 个字符）后解析剩余的数字部分。
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

/// @brief 根据 uname machine 字符串映射为 Architecture 枚举
/// @param machine 形如 "x86_64" / "aarch64" / "riscv64" 的机器架构标识
/// @return 对应的 Architecture 枚举值，未识别时返回 Architecture::Other
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

/// @brief 从路径映射中提取指定前缀下的唯一子目录键
/// @param path_map 路径到内容的映射（通常由 build_path_map 构建）
/// @param prefix 目录前缀，例如 "/sys/block/"
/// @return 去重并按字典序排序后的子目录名列表
/// @details 针对形如 "/sys/block/sda/size" 的路径，提取前缀之后的第一个
///          '/' 之前的片段作为子目录键（这里即为 "sda"）。
///          忽略没有下一级 '/' 的路径。结果去重并排序，保证输出稳定。
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
        // 去掉前缀后剩下的相对路径
        auto rest = std::string_view(path).substr(prefix.size());
        auto slash = rest.find('/');
        if(slash == std::string_view::npos)
        {
            // 没有子路径，跳过
            continue;
        }
        // 取第一个 '/' 之前的部分作为键
        auto key = std::string(rest.substr(0, slash));
        if(std::find(keys.begin(), keys.end(), key) == keys.end())
        {
            keys.push_back(key);
        }
    }
    // 排序以保证结果稳定、可预测
    std::sort(keys.begin(), keys.end());
    return keys;
}

} // namespace sysal::detail
