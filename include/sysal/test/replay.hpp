/// @file test/replay.hpp
/// @brief 测试回放接口
/// @details 提供基于原始数据存储（RawStore）的加载、保存与回放采集能力，
///          用于测试与解析器行为验证。

#pragma once

#include "sysal/collect_spec.hpp"
#include "sysal/error.hpp"
#include "sysal/expected.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/system_snapshot.hpp"

#include <string>

namespace sysal::test
{

/// @brief 从文件加载原始数据存储
/// @param path 文件路径
/// @return 成功返回 RawStore，失败返回错误
Expected<RawStore, SysalError> load_raw_store(const std::string& path);

/// @brief 基于原始数据回放采集系统快照
/// @param raw 原始数据存储
/// @param spec 采集规格
/// @return 成功返回系统快照，失败返回错误
Expected<SystemSnapshot, SysalError> collect_from_raw(const RawStore& raw,
                                                      const CollectSpec& spec = {});

/// @brief 将原始数据存储保存到文件
/// @param raw 原始数据存储
/// @param path 目标文件路径
/// @return 成功返回 void，失败返回错误
Expected<void, SysalError> save_raw_store(const RawStore& raw, const std::string& path);

} // namespace sysal::test
