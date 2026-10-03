/// @file file_utils.hpp
/// @brief 文件与命令读取工具
/// @details 提供 read_file / read_command / file_exists / add_record 等工具函数，
///          供 procfs / sysfs reader 使用，将原始数据写入 RawStore。

#pragma once

#include "reader/linux/command.hpp"
#include "sysal/model/raw_store.hpp"

#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace sysal::reader
{

    /// @brief 读取文件全部内容
    /// @param path 文件路径
    /// @return 文件内容；失败返回 nullopt
    inline std::optional<std::string> read_file(const std::string &path, int *error = nullptr)
    {
        errno = 0;
        std::ifstream ifs(path);
        if(!ifs)
        {
            if(error)
                *error = errno == 0 ? EIO : errno;
            return std::nullopt;
        }
        std::ostringstream oss;
        errno = 0;
        oss << ifs.rdbuf();
        if((!ifs && !ifs.eof()) || errno != 0 || oss.bad())
        {
            if(error)
                *error = errno == 0 ? EIO : errno;
            return std::nullopt;
        }
        return oss.str();
    }

    /// @brief 执行命令并读取标准输出
    /// @param cmd 要执行的命令
    /// @return 命令标准输出；失败返回 nullopt
    inline std::optional<std::string> read_command(const std::string &cmd)
    {
        auto result = execute_command(cmd);
        if(!result.successful() || result.output.empty())
            return std::nullopt;
        return std::move(result.output);
    }

    /// @brief 检查文件是否存在
    /// @param path 文件路径
    /// @return 存在返回 true
    inline bool file_exists(const std::string &path)
    {
        return std::filesystem::exists(path);
    }

    /// @brief 向 RawStore 添加一条记录
    /// @param raw 原始证据存储
    /// @param source 原始数据来源
    /// @param path_or_command 次级键
    /// @param payload 原始内容
    /// @param status 采集状态
    inline void add_record(RawStore &raw, RawSource source, const std::string &path_or_command,
                           const std::string &payload, CollectStatus status, std::optional<ReadFailure> failure = {})
    {
        raw.records.push_back(
            RawRecord{source, path_or_command, payload, status, std::chrono::system_clock::now(), failure});
    }

    inline std::optional<ReadFailure> command_failure(const CommandResult &result)
    {
        if(result.status == CommandStatus::TimedOut)
            return ReadFailure::TimedOut;
        if(result.exit_code == 127)
            return ReadFailure::ToolUnavailable;
        if(result.error.find("Operation not permitted") != std::string::npos ||
           result.error.find("Permission denied") != std::string::npos)
            return ReadFailure::PermissionDenied;
        if(result.error.find("not supported") != std::string::npos)
            return ReadFailure::Unsupported;
        return result.successful() ? std::nullopt : std::optional{ReadFailure::IoError};
    }

    inline ReadFailure file_failure(int error)
    {
        if(error == EACCES || error == EPERM)
            return ReadFailure::PermissionDenied;
        if(error == ENOENT || error == ENOTDIR)
            return ReadFailure::NotPresent;
        if(error == EOPNOTSUPP || error == ENODEV || error == ENXIO || error == EINVAL)
            return ReadFailure::Unsupported;
        return ReadFailure::IoError;
    }

    inline void read_file_record(RawStore &raw, RawSource source, const std::string &path)
    {
        int error = 0;
        auto content = read_file(path, &error);
        add_record(raw, source, path, content.value_or(""), content ? CollectStatus::Success : CollectStatus::Failed,
                   content ? std::nullopt : std::optional{file_failure(error)});
    }

    inline std::string shell_argument(std::string_view value)
    {
        std::string result = "'";
        for(char ch : value)
            result += ch == '\'' ? "'\\''" : std::string(1, ch);
        return result + "'";
    }

} // namespace sysal::reader
