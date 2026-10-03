#pragma once

#include "sysal/model/memory.hpp"
#include "sysal/model/raw_store.hpp"

#include <string_view>

namespace sysal::detail
{
    std::optional<std::uint32_t> edac_controller_index(std::string_view path);
    void apply_memory_topology(Memory &memory, const RawStore &raw);
} // namespace sysal::detail
