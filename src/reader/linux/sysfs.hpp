/// @file sysfs.hpp
/// @brief Linux sysfs 采集器
/// @details 声明 read_sysfs 函数，从 /sys 采集原始数据写入 RawStore。

#pragma once

#include "sysal/core/collect.hpp"
#include "sysal/model/raw_store.hpp"

namespace sysal::reader
{

/// @brief 采集 sysfs 来源的原始数据
/// @param raw 原始证据存储
/// @param flags 采集范围位掩码
void read_sysfs(RawStore& raw, Collect flags);

} // namespace sysal::reader
