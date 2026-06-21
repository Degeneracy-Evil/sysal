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

inline std::optional<std::string> read_file(const std::string& path)
{
    std::ifstream file(path);
    if(!file)
    {
        return std::nullopt;
    }
    std::string content;
    std::string line;
    while(std::getline(file, line))
    {
        content += line;
        content += '\n';
    }
    return content;
}

inline std::optional<std::string> read_command(const std::string& cmd)
{
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if(!pipe)
    {
        return std::nullopt;
    }
    std::string content;
    std::array<char, 4096> buffer{};
    while(fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr)
    {
        content += buffer.data();
    }
    return content;
}

inline void add_record(RawStore& raw, RawSource source, std::string path,
                       const std::optional<std::string>& content,
                       std::chrono::system_clock::time_point now)
{
    raw.records.push_back({
        .source = source,
        .path_or_command = std::move(path),
        .payload = content.value_or(""),
        .status = content ? CollectStatus::Success : CollectStatus::Failed,
        .collected_at = now,
    });
}

} // namespace sysal::detail
