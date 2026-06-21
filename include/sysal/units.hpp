#pragma once

#include <cstdint>

namespace sysal
{

template <typename Tag> struct ScalarUnit
{
    std::uint64_t value{};

    constexpr bool operator==(const ScalarUnit& other) const { return value == other.value; }
};

struct MemorySizeTag
{
};
struct FrequencyTag
{
};
struct BandwidthTag
{
};

using MemorySize = ScalarUnit<MemorySizeTag>;
using Frequency = ScalarUnit<FrequencyTag>;
using Bandwidth = ScalarUnit<BandwidthTag>;

} // namespace sysal
