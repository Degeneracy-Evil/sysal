#pragma once

#include "sysal/error.hpp"
#include "sysal/expected.hpp"
#include "sysal/system_snapshot.hpp"

#include <string>
#include <string_view>

namespace sysal
{

struct SerializationOptions
{
    bool pretty_print = false;
    bool include_raw = false;
    bool include_meta = true;
};

std::string to_json(const SystemSnapshot& snapshot, const SerializationOptions& opts = {});

Expected<SystemSnapshot, SysalError> from_json(std::string_view json);

} // namespace sysal
