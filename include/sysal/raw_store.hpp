#pragma once

#include "sysal/enums.hpp"

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace sysal
{

struct RawRecord
{
    RawSource source;
    std::string path_or_command;
    std::string payload;
    CollectStatus status;
    std::chrono::system_clock::time_point collected_at;
};

struct RawStore
{
    std::vector<RawRecord> records;

    std::vector<const RawRecord*> get_all(RawSource source) const;
    std::vector<const RawRecord*> get(RawSource source, std::string_view path_or_command) const;
    bool has(RawSource source) const;
    std::size_t count(RawSource source) const;
};

} // namespace sysal
