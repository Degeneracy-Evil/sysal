/// @file version.hpp
/// @brief sysal 版本定义
/// @details 集中管理 sysal 版本号，供运行时元数据、序列化兼容性检查和构建系统引用。

#pragma once

namespace sysal
{

/// @brief 主版本号
inline constexpr int VERSION_MAJOR = 0;

/// @brief 次版本号
inline constexpr int VERSION_MINOR = 0;

/// @brief 修订版本号
inline constexpr int VERSION_PATCH = 2;

/// @brief 完整版本字符串（如 "0.0.1"）
inline constexpr const char* VERSION_STRING = "0.0.2";

} // namespace sysal
