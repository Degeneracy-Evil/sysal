/// @file raw_store.hpp
/// @brief 原始数据存储
/// @details 保存采集得到的原始系统证据记录，并按来源与路径提供查询接口。

#pragma once

#include "sysal/enums.hpp"

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace sysal
{

/// @brief 单条原始数据记录
struct RawRecord
{
    RawSource source;                                   ///< 数据来源
    std::string path_or_command;                        ///< 文件路径或执行命令
    std::string payload;                                ///< 原始负载内容
    CollectStatus status;                               ///< 采集状态
    std::chrono::system_clock::time_point collected_at; ///< 采集时间点
};

/// @brief 原始数据存储
/// @details 持有全部原始记录的集合，提供按来源与路径的检索能力。
struct RawStore
{
    std::vector<RawRecord> records; ///< 原始记录集合

    /// @brief 获取指定来源的全部记录
    /// @param source 数据来源
    /// @return 指向匹配记录的指针列表
    std::vector<const RawRecord*> get_all(RawSource source) const;
    /// @brief 按来源与路径/命令获取记录
    /// @param source 数据来源
    /// @param path_or_command 文件路径或命令字符串
    /// @return 指向匹配记录的指针列表
    std::vector<const RawRecord*> get(RawSource source, std::string_view path_or_command) const;
    /// @brief 判断是否包含指定来源的记录
    /// @param source 数据来源
    /// @return 存在时返回 true
    bool has(RawSource source) const;
    /// @brief 统计指定来源的记录数量
    /// @param source 数据来源
    /// @return 记录数量
    std::size_t count(RawSource source) const;
};

} // namespace sysal
