#pragma once

#include "sysal/model/memory.hpp"
#include "sysal/model/raw_store.hpp"

#include <string_view>

namespace sysal::detail
{
    std::optional<std::uint32_t> edac_controller_index(std::string_view path);
    EdacLocation parse_edac_location(std::string_view report);
    void apply_edac_inventory(Memory &memory, const RawStore &raw, std::string &memory_type);
    void apply_memory_topology(Memory &memory, const RawStore &raw);
} // namespace sysal::detail
