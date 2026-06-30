/// @file error.hpp
/// @brief sysal 错误类型定义
/// @details 定义统一的错误分类枚举与异常类，用于采集、解析、序列化等
///          各阶段的错误报告。

#pragma once

#include <exception>
#include <string>
#include <utility>

namespace sysal
{

/// @brief 错误类别
enum class ErrorKind
{
    CollectionFailed,     ///< 采集失败
    PartialCollection,    ///< 部分采集失败
    ParseError,           ///< 解析错误
    IoError,              ///< 输入输出错误
    FileNotFound,         ///< 文件未找到
    PermissionDenied,     ///< 权限不足
    BackendUnavailable,   ///< 后端不可用
    BackendError,         ///< 后端内部错误
    SerializationError,   ///< 序列化错误
    DeserializationError, ///< 反序列化错误
    Unknown,              ///< 未知错误
};

/// @brief sysal 统一异常类
/// @details 继承自 std::exception，携带错误类别与可读消息，供抛出式 API 使用。
class SysalError : public std::exception
{
public:
    /// @brief 构造异常
    /// @param kind 错误类别
    /// @param message 可读错误消息
    SysalError(ErrorKind kind, std::string message) : kind_(kind), message_(std::move(message)) {}

    /// @brief 获取错误类别
    /// @return 错误类别
    ErrorKind kind() const { return kind_; }
    /// @brief 获取错误消息
    /// @return 错误消息的只读引用
    const std::string& message() const { return message_; }
    /// @brief 获取 C 风格错误描述
    /// @return 错误消息的 C 字符串
    const char* what() const noexcept override { return message_.c_str(); }

private:
    ErrorKind kind_;      ///< 错误类别
    std::string message_; ///< 可读错误消息
};

} // namespace sysal
