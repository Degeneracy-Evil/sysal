/// @file pipeline.hpp
/// @brief 收集流水线接口
/// @details 声明 run_pipeline 函数，编排从 RawStore 到 SystemSnapshot 的完整流程：
///          parser 阶段将原始记录解析为 ParsedFacts，resolver 阶段将其组装为最终快照。

#pragma once

#include "sysal/collect_spec.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/system_snapshot.hpp"

#include <chrono>

namespace sysal::detail
{

/// @brief 运行解析-解析-解析流水线，生成系统快照
/// @param raw 已采集的原始数据存储
/// @param spec 采集规格，决定解析哪些类别
/// @param start_time 流水线起始时间点，用于填充快照元数据
/// @return 组装完成的 SystemSnapshot
SystemSnapshot run_pipeline(const RawStore& raw, const CollectSpec& spec,
                            std::chrono::system_clock::time_point start_time);

} // namespace sysal::detail
