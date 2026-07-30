/// @file pipeline.hpp
/// @brief 采集管线编排
/// @details 声明 run_pipeline 和 run_replay 函数，编排
///          Reader → Parser → Resolver 的完整流程。

#pragma once

#include "sysal/core/collect.hpp"
#include "sysal/core/system.hpp"
#include "sysal/model/raw_store.hpp"

#include <string>
#include <vector>

namespace sysal::detail
{

    /// @brief 执行完整采集管线：Reader → Parser → Resolver
    /// @param flags 采集范围位掩码
    /// @param warnings 警告列表（追加写入）
    /// @return 采集结果
    System run_pipeline(Collect flags, std::vector<std::string> &warnings);

    /// @brief 从已有 RawStore 执行回放管线：Parser → Resolver
    /// @param raw 原始证据存储
    /// @param flags 采集范围位掩码
    /// @param warnings 警告列表（追加写入）
    /// @return 采集结果
    System run_replay(const RawStore &raw, Collect flags, std::vector<std::string> &warnings);

} // namespace sysal::detail
