/// @file accelerator.hpp
/// @brief 加速器信息解析器
/// @details 从 RawStore 中解析 Accelerators 结构体，包括 GPU、NPU、FPGA
///          等异构加速设备信息。

#pragma once

#include "sysal/model/accelerator.hpp"
#include "sysal/model/raw_store.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal::detail
{

/// @brief 解析加速器信息
/// @param raw 原始证据存储（只读）
/// @param warnings 警告列表（追加写入）
/// @return 解析成功返回 Accelerators，否则返回 nullopt
std::optional<Accelerators> parse_accelerator(const RawStore& raw,
                                              std::vector<std::string>& warnings);

} // namespace sysal::detail
