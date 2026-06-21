#pragma once

#include <cstdint>
#include <functional>
#include <ostream>

namespace sysal
{

/// Type-safe identifier wrapper. Different StrongId instantiations are not
/// implicitly convertible to each other or to the underlying type.
///
/// The Tag parameter distinguishes semantically distinct IDs that share the
/// same underlying representation (e.g. CpuPackageId vs CpuCoreId).
template <typename T, typename Tag> class StrongId
{
public:
    StrongId() = default;
    explicit constexpr StrongId(T value) : value_(value) {}

    constexpr T value() const { return value_; }

    constexpr bool operator==(const StrongId& other) const { return value_ == other.value_; }
    constexpr bool operator!=(const StrongId& other) const { return value_ != other.value_; }

    friend std::ostream& operator<<(std::ostream& os, const StrongId& id)
    {
        return os << id.value_;
    }

private:
    T value_{};
};

} // namespace sysal

namespace std
{

template <typename T, typename Tag> struct hash<sysal::StrongId<T, Tag>>
{
    std::size_t operator()(const sysal::StrongId<T, Tag>& id) const noexcept
    {
        return std::hash<T>{}(id.value());
    }
};

} // namespace std
