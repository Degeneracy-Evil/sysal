#pragma once
#include "sysal/model/filesystem.hpp"
#include <string_view>
namespace sysal::detail
{
    FilesystemKind filesystem_kind(std::string_view type);
}
