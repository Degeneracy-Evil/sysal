/// @file file_utils.hpp
/// @brief 文件与命令读取工具
/// @details 提供 read_file / read_command / file_exists / add_record 等工具函数，
///          供 procfs / sysfs reader 使用，将原始数据写入 RawStore。

#pragma once

#include "sysal/model/raw_store.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

namespace sysal::reader
{

    /// @brief 读取文件全部内容
    /// @param path 文件路径
    /// @return 文件内容；失败返回 nullopt
    inline std::optional<std::string> read_file(const std::string &path)
    {
        std::ifstream ifs(path);
        if(!ifs)
        {
            return std::nullopt;
        }
        std::ostringstream oss;
        oss << ifs.rdbuf();
        if(!ifs && !ifs.eof())
        {
            return std::nullopt;
        }
        return oss.str();
    }

    /// @brief 执行命令并读取标准输出
    /// @param cmd 要执行的命令
    /// @return 命令标准输出；失败返回 nullopt
    inline std::optional<std::string> read_command(const std::string &cmd)
    {
        // 重定向 stderr 到 /dev/null，避免命令不存在时错误信息泄漏到终端
        std::string full_cmd = cmd + " 2>/dev/null";
        // NOLINTNEXTLINE(cert-env33-c) — 采集层需要执行外部命令
        auto *pipe = popen(full_cmd.c_str(), "r");
        if(!pipe)
        {
            return std::nullopt;
        }
        std::ostringstream oss;
        char buffer[256];
        while(auto *ptr = std::fgets(buffer, sizeof(buffer), pipe))
        {
            oss << ptr;
        }
        auto status = pclose(pipe);
        if(status == -1)
        {
            return std::nullopt;
        }
        auto output = oss.str();
        if(output.empty())
        {
            return std::nullopt;
        }
        return output;
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
                           const std::string &payload, CollectStatus status)
    {
        raw.records.push_back(RawRecord{source, path_or_command, payload, status, std::chrono::system_clock::now()});
    }

} // namespace sysal::reader
