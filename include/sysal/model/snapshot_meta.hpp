/// @file snapshot_meta.hpp
/// @brief 采集元数据
/// @details 定义 SnapshotMeta 结构体，记录采集时刻、版本、耗时、请求域
///          及成功/失败的采集器列表。

#pragma once

#include "sysal/core/collect.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace sysal
{

    /// @brief 采集元数据
    struct SnapshotMeta
    {
        std::chrono::system_clock::time_point collect_time; ///< 采集时刻
        std::string sysal_version;                          ///< sysal 版本
        std::chrono::duration<double> collect_duration;     ///< 采集耗时
        Collect requested_flags;                            ///< 请求的采集域
        std::vector<std::string> succeeded_collectors;      ///< 成功的采集器
        std::vector<std::string> failed_collectors;         ///< 失败的采集器
    };

} // namespace sysal
