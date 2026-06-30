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
    CollectionFailed,     ///< 采集失败
    ParseError,           ///< 解析错误
    IoError,              ///< I/O 错误
    FileNotFound,         ///< 文件未找到
    PermissionDenied,     ///< 权限不足
    BackendUnavailable,   ///< 后端不可用
    BackendError,         ///< 后端错误
    SerializationError,   ///< 序列化错误
    DeserializationError, ///< 反序列化错误
    Unknown               ///< 未知错误
};

/// @brief sysal 异常类
/// @details 当采集彻底失败（而非部分子域失败）时抛出。
class SysalError : public std::exception
{
public:
    SysalError(ErrorKind kind, std::string_view message)
        : kind_(kind), message_(std::string{message})
    {
    }

    [[nodiscard]] const char* what() const noexcept override { return message_.c_str(); }

    [[nodiscard]] ErrorKind kind() const noexcept { return kind_; }

private:
    ErrorKind kind_;
    std::string message_;
};

} // namespace sysal
