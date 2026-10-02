#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace sysal::reader
{
    enum class CommandStatus
    {
        Success,
        Failed,
        SpawnError,
        TimedOut,
        OutputLimit,
        ReadError
    };

    struct CommandOptions
    {
        std::chrono::milliseconds timeout{3000};
        std::size_t output_limit{std::size_t{16} * 1024 * 1024};
    };

    struct CommandResult
    {
        CommandStatus status{CommandStatus::SpawnError};
        std::optional<int> exit_code;
        std::optional<int> signal;
        std::string output;
        std::string error;
        [[nodiscard]] bool successful() const
        {
            return status == CommandStatus::Success;
        }
    };

    CommandResult execute_command(const std::string &command, const CommandOptions &options = {});
} // namespace sysal::reader
