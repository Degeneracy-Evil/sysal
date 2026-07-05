/// @file units.hpp
/// @brief 强类型标量单位
/// @details 通过幻影标签（Tag）区分语义不同的标量量纲，避免不同单位的
///          uint64 值相互误用。

#pragma once

#include <cstdint>

namespace sysal
{

/// @brief 带幻影标签的标量单位模板
/// @details 不同 Tag 实例化为不同类型，不可隐式转换，底层统一为 uint64。
/// @tparam Tag 幻影标签类型，用于区分量纲
template <typename Tag> struct ScalarUnit
{
    std::uint64_t value{}; ///< 原始标量值

    /// @brief 相等比较
    /// @param other 另一个同类型单位
    /// @return 值相等时返回 true
    constexpr bool operator==(const ScalarUnit& other) const { return value == other.value; }
};

/// @brief 内存大小量纲标签
struct MemorySizeTag
{
};
/// @brief 频率量纲标签
struct FrequencyTag
{
};
/// @brief 带宽量纲标签
struct BandwidthTag
{
};
/// @brief 传输速率量纲标签
struct TransferRateTag
{
};

using MemorySize = ScalarUnit<MemorySizeTag>;     ///< 内存大小（字节）
using Frequency = ScalarUnit<FrequencyTag>;       ///< 频率（赫兹）
using Bandwidth = ScalarUnit<BandwidthTag>;       ///< 带宽（比特每秒）
using TransferRate = ScalarUnit<TransferRateTag>; ///< 传输速率（MT/s）

} // namespace sysal
