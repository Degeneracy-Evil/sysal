/// @file diagnostics.hpp
/// @brief 诊断信息数据模型
/// @details 记录采集、解析、解析冲突解决过程中产生的诊断信息。

#pragma once

#include "sysal/enums.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal
{

/// @brief 数据冲突详情
/// @details 当不同来源对同一字段给出不同值时，记录高优先级与低优先级来源及其值。
struct ConflictDetail
{
    std::string field;             ///< 冲突字段名
    std::string value_from_higher; ///< 高优先级来源提供的值
    std::string value_from_lower;  ///< 低优先级来源提供的值
    RawSource higher_source;       ///< 高优先级来源
    RawSource lower_source;        ///< 低优先级来源
};

/// @brief 单条诊断记录
struct Diagnostic
{
    Severity severity;                      ///< 严重级别
    std::string message;                    ///< 可读诊断消息
    std::optional<RawSource> source;        ///< 相关数据来源（可能无）
    std::optional<ConflictDetail> conflict; ///< 冲突详情（仅冲突诊断）
};

/// @brief 诊断信息集合
struct Diagnostics
{
    std::vector<Diagnostic> records; ///< 诊断记录列表
};

} // namespace sysal
