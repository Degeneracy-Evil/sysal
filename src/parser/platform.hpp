/// @file platform.hpp
/// @brief 平台信息解析器
/// @details 从 RawStore 中解析 Platform 结构体，包括主机、操作系统、
///          内核、架构、固件和虚拟化信息。

#pragma once

#include "sysal/model/platform.hpp"
#include "sysal/model/raw_store.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal::detail
{

    /// @brief 解析平台信息
    /// @param raw 原始证据存储（只读）
    /// @param warnings 警告列表（追加写入）
    /// @return 解析成功返回 Platform，否则返回 nullopt
    std::optional<Platform> parse_platform(const RawStore &raw, std::vector<std::string> &warnings);

} // namespace sysal::detail
