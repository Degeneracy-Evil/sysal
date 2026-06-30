/// @file expected.hpp
/// @brief 简化的 Expected 类型
/// @details 提供轻量级的 Expected<T, E> 与 Expected<void, E>，用于非抛出式
///          API 返回值或错误。功能上对标 std::expected 的最小子集。

#pragma once

#include <optional>
#include <utility>

namespace sysal
{

/// @brief 包装错误值的载体类型
/// @tparam E 错误类型
template <typename E> struct Unexpected
{
    E error; ///< 被包装的错误值
};

/// @brief 创建 Unexpected 对象的工厂函数
/// @tparam E 错误类型
/// @param error 错误值
/// @return 包装给定错误的 Unexpected
template <typename E> Unexpected<E> make_unexpected(E error)
{
    return Unexpected<E>{std::move(error)};
}

/// @brief 期望值类型，持有成功值或错误值
/// @tparam T 成功值类型
/// @tparam E 错误类型
template <typename T, typename E> class Expected
{
public:
    /// @brief 从成功值构造（左值）
    /// @param val 成功值
    Expected(const T& val) : value_(val) {}
    /// @brief 从成功值构造（右值）
    /// @param val 成功值
    Expected(T&& val) : value_(std::move(val)) {}
    /// @brief 从错误值构造
    /// @param unex 包装的错误值
    Expected(Unexpected<E> unex) : error_(std::move(unex.error)) {}

    Expected(const Expected&) = default;
    Expected(Expected&&) = default;
    Expected& operator=(const Expected&) = default;
    Expected& operator=(Expected&&) = default;

    /// @brief 判断是否持有成功值
    /// @return 持有成功值时返回 true
    bool has_value() const { return value_.has_value(); }
    /// @brief 判断是否持有成功值
    /// @return 持有成功值时返回 true
    explicit operator bool() const { return has_value(); }

    /// @brief 解引用获取成功值（只读）
    /// @return 成功值的只读引用
    const T& operator*() const { return *value_; }
    /// @brief 解引用获取成功值
    /// @return 成功值的可写引用
    T& operator*() { return *value_; }
    /// @brief 获取成功值
    /// @return 成功值的只读引用
    const T& value() const { return *value_; }

    /// @brief 获取错误值
    /// @return 错误值的只读引用
    const E& error() const { return *error_; }

private:
    std::optional<T> value_; ///< 成功值
    std::optional<E> error_; ///< 错误值
};

/// @brief Expected<void, E> 特化，表示无返回值但可能失败的操作
/// @tparam E 错误类型
template <typename E> class Expected<void, E>
{
public:
    Expected() = default;
    /// @brief 从错误值构造
    /// @param unex 包装的错误值
    Expected(Unexpected<E> unex) : error_(std::move(unex.error)) {}

    /// @brief 判断是否成功（无错误）
    /// @return 无错误时返回 true
    bool has_value() const { return !error_.has_value(); }
    /// @brief 判断是否成功（无错误）
    /// @return 无错误时返回 true
    explicit operator bool() const { return has_value(); }

    /// @brief 获取错误值
    /// @return 错误值的只读引用
    const E& error() const { return *error_; }

private:
    std::optional<E> error_; ///< 错误值
};

} // namespace sysal
