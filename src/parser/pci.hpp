/// @file pci.hpp
/// @brief PCI 信息解析器
/// @details 从 RawStore 中解析 Pci 结构体，包括 PCI 设备地址、
///          厂商、设备名、类别和 NUMA 节点信息。

#pragma once

#include "sysal/model/pci.hpp"
#include "sysal/model/raw_store.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal::detail
{

/// @brief 解析 PCI 信息
/// @param raw 原始证据存储（只读）
/// @param warnings 警告列表（追加写入）
/// @return 解析成功返回 Pci，否则返回 nullopt
std::optional<Pci> parse_pci(const RawStore& raw, std::vector<std::string>& warnings);

} // namespace sysal::detail
