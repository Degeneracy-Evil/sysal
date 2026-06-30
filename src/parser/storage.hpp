/// @file storage.hpp
/// @brief 存储信息解析器
/// @details 从 RawStore 中解析 Storage 结构体，包括块设备名称、容量、
///          类型与 PCI 地址。

#pragma once

#include "sysal/model/raw_store.hpp"
#include "sysal/model/storage.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal::detail
{

/// @brief 解析存储信息
/// @param raw 原始证据存储（只读）
/// @param warnings 警告列表（追加写入）
/// @return 解析成功返回 Storage，否则返回 nullopt
std::optional<Storage> parse_storage(const RawStore& raw, std::vector<std::string>& warnings);

} // namespace sysal::detail
