/// @file snapshot_meta.hpp
/// @brief 快照元数据
/// @details 记录一次采集的时间、版本、耗时、请求规格及各采集器的成败情况。

#pragma once

#include "sysal/collect_spec.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace sysal
{

/// @brief 系统快照元数据
struct SnapshotMeta
{
    std::chrono::system_clock::time_point collect_time; ///< 采集时间点
    std::string sysal_version;                          ///< 生成快照的 sysal 版本
    std::chrono::milliseconds collect_duration{};       ///< 采集耗时
    CollectSpec requested_spec;                         ///< 请求的采集规格
    std::vector<std::string> succeeded_collectors;      ///< 成功的采集器名称列表
    std::vector<std::string> failed_collectors;         ///< 失败的采集器名称列表
};

} // namespace sysal
