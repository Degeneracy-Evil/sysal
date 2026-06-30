/// @file network.hpp
/// @brief 网络信息解析器
/// @details 从 RawStore 中解析 Network 结构体，包括网络接口名称、
///          MAC 地址、链路状态和速率信息。

#pragma once

#include "sysal/model/network.hpp"
#include "sysal/model/raw_store.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal::detail
{

/// @brief 解析网络信息
/// @param raw 原始证据存储（只读）
/// @param warnings 警告列表（追加写入）
/// @return 解析成功返回 Network，否则返回 nullopt
std::optional<Network> parse_network(const RawStore& raw, std::vector<std::string>& warnings);

} // namespace sysal::detail
