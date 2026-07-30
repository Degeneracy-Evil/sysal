/// @file procfs.hpp
/// @brief Linux procfs 采集器
/// @details 声明 read_procfs 函数，从 /proc、/etc、外部命令及环境变量
///          采集原始数据写入 RawStore。

#pragma once

#include "sysal/core/collect.hpp"
#include "sysal/model/raw_store.hpp"

namespace sysal::reader
{

    /// @brief 采集 procfs 及相关来源的原始数据
    /// @param raw 原始证据存储
    /// @param flags 采集范围位掩码
    void read_procfs(RawStore &raw, Collect flags);

} // namespace sysal::reader
