/// @file parse_utils.hpp
/// @brief 解析工具函数
/// @details 提供 Parser 共享的字符串处理与解析辅助函数。

#pragma once

#include "sysal/types/enums.hpp"
#include "sysal/types/ids.hpp"
#include "sysal/types/units.hpp"
#include "sysal/types/value_types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sysal::detail
{

/// @brief 去除首尾空白
std::string trim(std::string_view s);

/// @brief 按分隔符拆分
std::vector<std::string> split(std::string_view s, char delimiter);

/// @brief 解析 key: value 格式的键值对
/// @return pair<key, value>，若找不到分隔符则 key 为 trim 后的整行，value 为空
std::pair<std::string, std::string> parse_kv(std::string_view line, char separator = ':');

/// @brief 解析无符号整数
std::optional<std::uint64_t> parse_uint(std::string_view s);

/// @brief 解析十六进制无符号整数
std::optional<std::uint64_t> parse_hex(std::string_view s);

/// @brief 解析 PCI 地址（十六进制格式 DDDD:BB:DD.F）
std::optional<PciAddress> parse_pci_address(std::string_view s);

/// @brief 将 KB 单位的值转换为字节数
std::optional<MemorySize> parse_kb_to_bytes(std::string_view s);

} // namespace sysal::detail
