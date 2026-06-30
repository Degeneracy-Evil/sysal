/// @file raw_store.hpp
/// @brief 原始证据存储数据模型
/// @details 定义 RawRecord、RawStore，存储从系统采集的原始证据，
///          并提供按来源与次级键的查询接口。

#pragma once

#include "sysal/types/enums.hpp"

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace sysal
{

/// @brief 单条原始记录
struct RawRecord
{
    RawSource source;                                   ///< 原始数据来源
    std::string path_or_command;                        ///< 次级键
    std::string payload;                                ///< 原始内容
    CollectStatus status;                               ///< 采集状态
    std::chrono::system_clock::time_point collected_at; ///< 采集时刻
};

/// @brief 原始证据存储
/// @details 持有全部原始记录，并提供按来源与次级键的查询接口。
struct RawStore
{
    std::vector<RawRecord> records; ///< 原始记录列表

    /// @brief 获取指定来源的全部记录
    /// @param source 原始数据来源
    /// @return 指向匹配记录的指针向量
    std::vector<const RawRecord*> get_all(RawSource source) const;

    /// @brief 获取指定来源与次级键的记录
    /// @param source 原始数据来源
    /// @param path_or_command 次级键
    /// @return 指向匹配记录的指针向量
    std::vector<const RawRecord*> get(RawSource source, std::string_view path_or_command) const;

    /// @brief 判断是否存在指定来源的记录
    /// @param source 原始数据来源
    /// @return 存在则返回 true
    bool has(RawSource source) const;

    /// @brief 统计指定来源的记录数
    /// @param source 原始数据来源
    /// @return 记录数
    std::size_t count(RawSource source) const;
};

} // namespace sysal
