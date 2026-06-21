#pragma once

#include <optional>
#include <utility>

namespace sysal
{

template <typename E> struct Unexpected
{
    E error;
};

template <typename E> Unexpected<E> make_unexpected(E error)
{
    return Unexpected<E>{std::move(error)};
}

template <typename T, typename E> class Expected
{
public:
    Expected(const T& val) : value_(val) {}
    Expected(T&& val) : value_(std::move(val)) {}
    Expected(Unexpected<E> unex) : error_(std::move(unex.error)) {}

    Expected(const Expected&) = default;
    Expected(Expected&&) = default;
    Expected& operator=(const Expected&) = default;
    Expected& operator=(Expected&&) = default;

    bool has_value() const { return value_.has_value(); }
    explicit operator bool() const { return has_value(); }

    const T& operator*() const { return *value_; }
    T& operator*() { return *value_; }
    const T& value() const { return *value_; }

    const E& error() const { return *error_; }

private:
    std::optional<T> value_;
    std::optional<E> error_;
};

template <typename E> class Expected<void, E>
{
public:
    Expected() = default;
    Expected(Unexpected<E> unex) : error_(std::move(unex.error)) {}

    bool has_value() const { return !error_.has_value(); }
    explicit operator bool() const { return has_value(); }

    const E& error() const { return *error_; }

private:
    std::optional<E> error_;
};

} // namespace sysal
