#pragma once

#include <exception>
#include <string>
#include <utility>

namespace sysal
{

enum class ErrorKind
{
    CollectionFailed,
    PartialCollection,
    ParseError,
    IoError,
    FileNotFound,
    PermissionDenied,
    BackendUnavailable,
    BackendError,
    SerializationError,
    DeserializationError,
    Unknown,
};

class SysalError : public std::exception
{
public:
    SysalError(ErrorKind kind, std::string message) : kind_(kind), message_(std::move(message)) {}

    ErrorKind kind() const { return kind_; }
    const std::string& message() const { return message_; }
    const char* what() const noexcept override { return message_.c_str(); }

private:
    ErrorKind kind_;
    std::string message_;
};

} // namespace sysal
