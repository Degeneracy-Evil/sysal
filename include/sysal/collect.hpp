/// @file collect.hpp
/// @brief 系统信息采集入口
/// @details 提供非抛出式与抛出式两种采集 API，按给定规格收集系统快照。

#pragma once

#include "sysal/collect_spec.hpp"
#include "sysal/error.hpp"
#include "sysal/expected.hpp"
#include "sysal/system_snapshot.hpp"

namespace sysal
{

/// @brief 非抛出式采集系统快照
/// @param spec 采集规格
/// @return 成功返回快照，失败返回错误
Expected<SystemSnapshot, SysalError> collect(const CollectSpec& spec);
/// @brief 抛出式采集系统快照
/// @param spec 采集规格
/// @return 系统快照
/// @throws SysalError 采集失败时抛出
SystemSnapshot collect_or_throw(const CollectSpec& spec);

} // namespace sysal
