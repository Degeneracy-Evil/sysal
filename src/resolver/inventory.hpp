#pragma once

#include "sysal/core/system.hpp"

namespace sysal::detail
{
    std::vector<InventoryReport> inventory_reports(const SystemInfo &info, const RawStore &raw, Collect flags);
}
