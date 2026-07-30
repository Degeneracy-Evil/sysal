/// @file replay.hpp
/// @brief Raw replay 测试基础设施
/// @details 提供 RawStore 的 JSON 文件保存与加载，以及从 RawStore 直接采集，
///          用于 raw replay 测试策略。

#pragma once

#include "sysal/core/collect.hpp"
#include "sysal/core/system.hpp"
#include "sysal/model/raw_store.hpp"

#include <string>

namespace sysal::test
{

    /// @brief 从 JSON 文件加载原始数据
    /// @param path JSON 文件路径
    /// @return 加载后的 RawStore
    /// @throws SysalError 文件读取失败或 JSON 解析失败时抛出
    RawStore load_raw_store(const std::string &path);

    /// @brief 将原始数据保存到 JSON 文件
    /// @param raw 要保存的 RawStore
    /// @param path 目标文件路径
    /// @throws SysalError 文件写入失败时抛出
    void save_raw_store(const RawStore &raw, const std::string &path);

    /// @brief 从已有 RawStore 执行采集（Parser → Resolver）
    /// @param raw 原始证据存储
    /// @param flags 采集范围位掩码，默认为 full
    /// @return 采集结果
    System collect_from_raw(const RawStore &raw, Collect flags = full);

} // namespace sysal::test
