/// @file strong_id.hpp
/// @brief 强类型标识符包装
/// @details 提供 StrongId 模板，用于在类型层面区分语义不同的 ID，
///          防止相互误用，并支持哈希与流输出。

#pragma once

#include <cstdint>
#include <functional>
#include <ostream>

namespace sysal
{

/// @brief 类型安全的标识符包装
/// @details 不同 StrongId 实例化之间不会隐式转换，也不会与底层类型转换。
///          Tag 参数用于区分共享相同底层表示但语义不同的 ID
///          （如 CpuPackageId 与 CpuCoreId）。
/// @tparam T 底层值类型
/// @tparam Tag 幻影标签类型
template <typename T, typename Tag> class StrongId
{
public:
    StrongId() = default;
    /// @brief 从底层值显式构造
    /// @param value 底层值
    explicit constexpr StrongId(T value) : value_(value) {}

    /// @brief 获取底层值
    /// @return 底层值的副本
    constexpr T value() const { return value_; }

    /// @brief 相等比较
    constexpr bool operator==(const StrongId& other) const { return value_ == other.value_; }
    /// @brief 不等比较
    constexpr bool operator!=(const StrongId& other) const { return value_ != other.value_; }

    /// @brief 流输出底层值
    friend std::ostream& operator<<(std::ostream& os, const StrongId& id)
    {
        return os << id.value_;
    }

private:
    T value_{}; ///< 底层值，默认初始化
};

} // namespace sysal

namespace std
{

/// @brief StrongId 的 std::hash 特化
/// @details 使 StrongId 可用于 unordered 容器。
template <typename T, typename Tag> struct hash<sysal::StrongId<T, Tag>>
{
    /// @brief 计算哈希值
    /// @param id 标识符
    /// @return 底层值的哈希
    std::size_t operator()(const sysal::StrongId<T, Tag>& id) const noexcept
    {
        return std::hash<T>{}(id.value());
    }
};

} // namespace std
