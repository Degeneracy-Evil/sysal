#pragma once

#include "sysal/model/storage.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace sysal::detail
{
    std::vector<StorageMount> parse_mountinfo(std::string_view payload, std::vector<std::string> &warnings);
}
