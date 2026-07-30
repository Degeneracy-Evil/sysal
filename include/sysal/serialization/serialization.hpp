/// @file serialization.hpp
/// @brief System JSON 序列化与反序列化接口
/// @details 提供 to_json / from_json 自由函数，将 System 对象序列化为 JSON
///          字符串或从 JSON 字符串反序列化为 System 对象。非侵入式设计，
///          不在 System 上添加方法。

#pragma once

#include "sysal/core/system.hpp"

#include <string>
#include <string_view>

namespace sysal
{

    /// @brief 序列化选项
    struct SerializationOptions
    {
        bool pretty_print = false; ///< 是否美化输出（换行 + 缩进）
        bool include_raw = false;  ///< 是否输出 raw 字段（仅当 System::raw 有值时有效）
        bool include_meta = true;  ///< 是否输出 meta 字段
    };

    /// @brief 将 System 序列化为 JSON 字符串
    /// @param sys 采集结果
    /// @param opts 序列化选项
    /// @return JSON 文本字符串
    std::string to_json(const System &sys, const SerializationOptions &opts = {});

    /// @brief 从 JSON 字符串反序列化为 System
    /// @param json JSON 文本
    /// @return 反序列化后的 System 对象
    /// @throws SysalError JSON 语法错误、版本不兼容或必填字段缺失时抛出
    System from_json(std::string_view json);

} // namespace sysal
