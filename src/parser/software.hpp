/// @file software.hpp
/// @brief 软件栈解析器
/// @details 从 RawStore 中解析 SoftwareStack 结构体，包括 NVIDIA 驱动版本、
///          CUDA 运行时版本等软件栈信息。

#pragma once

#include "sysal/model/raw_store.hpp"
#include "sysal/model/software.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal::detail
{

/// @brief 解析软件栈信息
/// @param raw 原始证据存储（只读）
/// @param warnings 警告列表（追加写入）
/// @return 解析成功返回 SoftwareStack，否则返回 nullopt
std::optional<SoftwareStack> parse_software(const RawStore& raw,
                                            std::vector<std::string>& warnings);

} // namespace sysal::detail
