/// @file resolver.hpp
/// @brief 解析器接口声明
/// @details 声明从 ParsedFacts 构建最终 SystemSnapshot 的解析入口函数。

#pragma once

#include "parser/parsed_facts.hpp"
#include "sysal/collect_spec.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/system_snapshot.hpp"

#include <chrono>
#include <optional>
#include <utility>

namespace sysal::detail
{

/// @brief 从已解析事实构建最终的 SystemSnapshot
/// @details 将各采集器产生的 ParsedFacts 子字段移动到 SystemSnapshot 对应位置，
///          并根据执行上下文的 cpuset 计算逻辑 CPU、加速器、网络接口的可见性，
///          最终将可选的原始证据 RawStore 一并挂载到快照上。
/// @param facts 已解析的中间事实，按需子字段存在则被移动
/// @param spec 本次采集的规格，用于回填快照元数据
/// @param start_time 采集起始时刻，用于计算采集耗时
/// @param raw 可选的原始证据存储，直接挂载到快照
/// @return 构建完成的 SystemSnapshot
SystemSnapshot resolve(ParsedFacts&& facts, const CollectSpec& spec,
                       std::chrono::system_clock::time_point start_time,
                       const std::optional<RawStore>& raw);

} // namespace sysal::detail
