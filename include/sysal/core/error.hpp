/// @file error.hpp
/// @brief 错误类型
/// @details 定义 SysalError 异常类，在采集彻底失败时由 System::collect() 抛出。
///          部分子域失败不会抛出异常，而是记录到 System::warnings。

#pragma once

#include <exception>
#include <string>
#include <string_view>

namespace sysal
{

    /// @brief 错误类别
    enum class ErrorKind
    {
        CollectionFailed,
        ParseError,
        IoError,
        FileNotFound,
        PermissionDenied,
        BackendUnavailable,
        BackendError,
        SerializationError,
        DeserializationError,
        Unknown
    };

    /// @brief sysal 异常类
    /// @details 当采集彻底失败（而非部分子域失败）时抛出。
    class SysalError : public std::exception
    {
    public:
        /// @brief 构造异常
        /// @param kind 错误类别
        /// @param message 错误描述
        SysalError(ErrorKind kind, std::string_view message) : kind_(kind), message_(std::string{message}) {}

        /// @brief 获取错误描述
        /// @return C 风格字符串
        [[nodiscard]] const char *what() const noexcept override
        {
            return message_.c_str();
        }

        /// @brief 获取错误类别
        /// @return 错误类别枚举值
        [[nodiscard]] ErrorKind kind() const noexcept
        {
            return kind_;
        }

    private:
        ErrorKind kind_;
        std::string message_;
    };

} // namespace sysal
