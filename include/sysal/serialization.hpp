/// @file serialization.hpp
/// @brief 序列化与反序列化接口
/// @details 提供 SystemSnapshot 与 JSON 之间的转换接口，支持格式化输出、
///          是否包含原始数据与元数据等选项。

#pragma once

#include "sysal/error.hpp"
#include "sysal/expected.hpp"
#include "sysal/system_snapshot.hpp"

#include <string>
#include <string_view>

namespace sysal
{

/// @brief 序列化选项
struct SerializationOptions
{
    bool pretty_print = false; ///< 是否美化输出
    bool include_raw = false;  ///< 是否包含原始数据
    bool include_meta = true;  ///< 是否包含元数据
};

/// @brief 将系统快照序列化为 JSON 字符串
/// @param snapshot 系统快照
/// @param opts 序列化选项
/// @return JSON 字符串
std::string to_json(const SystemSnapshot& snapshot, const SerializationOptions& opts = {});

/// @brief 从 JSON 字符串反序列化为系统快照
/// @param json JSON 字符串
/// @return 成功返回快照，失败返回错误
Expected<SystemSnapshot, SysalError> from_json(std::string_view json);

} // namespace sysal
