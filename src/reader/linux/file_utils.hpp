/// @file file_utils.hpp
/// @brief Linux 文件与命令读取工具
/// @details 提供 read_file / read_command / add_record 等底层工具函数，
///          用于从 /proc、/sys 文件或外部命令获取原始文本内容，
///          并将结果以 RawRecord 形式追加到 RawStore 中。

#pragma once

#include "sysal/enums.hpp"
#include "sysal/raw_store.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace sysal::detail
{

/// @brief 读取文件全部内容
/// @param path 要读取的文件路径
/// @return 成功返回文件内容字符串（每行末尾补 '\n'），失败返回 std::nullopt
inline std::optional<std::string> read_file(const std::string& path)
{
    std::ifstream file(path);
    if(!file)
    {
        // 文件无法打开（不存在或无权限）
        return std::nullopt;
    }
    std::string content;
    std::string line;
    while(std::getline(file, line))
    {
        // 逐行读取并补回换行符，保留原始行结构
        content += line;
        content += '\n';
    }
    return content;
}

/// @brief 执行外部命令并捕获标准输出
/// @param cmd 要执行的 shell 命令字符串
/// @return 成功返回命令输出内容，popen 失败返回 std::nullopt
inline std::optional<std::string> read_command(const std::string& cmd)
{
    // 通过 popen 创建管道读取子进程输出，pclose 自动作为删除器
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if(!pipe)
    {
        // 管道创建失败
        return std::nullopt;
    }
    std::string content;
    std::array<char, 4096> buffer{};
    while(fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr)
    {
        // 分块读取直到子进程输出结束
        content += buffer.data();
    }
    return content;
}

/// @brief 向 RawStore 追加一条原始记录
/// @param raw 目标 RawStore 引用
/// @param source 数据来源标识
/// @param path 文件路径或命令字符串（记录的来源描述）
/// @param content 读取到的内容，为空表示采集失败
/// @param now 采集时间点
inline void add_record(RawStore& raw, RawSource source, std::string path,
                       const std::optional<std::string>& content,
                       std::chrono::system_clock::time_point now)
{
    raw.records.push_back({
        .source = source,
        .path_or_command = std::move(path),
        .payload = content.value_or(""), // 失败时 payload 留空字符串
        .status = content ? CollectStatus::Success : CollectStatus::Failed,
        .collected_at = now,
    });
}

} // namespace sysal::detail
