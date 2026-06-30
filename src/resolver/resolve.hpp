/// @file resolve.hpp
/// @brief 冲突解决与可见性计算
/// @details 声明 resolve 函数，将 ParseResult 合并为 SystemInfo，
///          解决冲突、计算可见性、交叉校验便利索引。

#pragma once

#include "parser/parse_result.hpp"

#include "sysal/core/system.hpp"

#include <string>
#include <vector>

namespace sysal::detail
{

/// @brief 合并 ParseResult，解决冲突，计算可见性，构建 SystemInfo
/// @param result 解析结果（右值引用，内部移动）
/// @param warnings 警告列表（追加写入）
/// @return 完整的 SystemInfo
SystemInfo resolve(ParseResult result, std::vector<std::string>& warnings);

} // namespace sysal::detail
