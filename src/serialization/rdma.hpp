#pragma once

#include "sysal/model/rdma.hpp"

#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json rdma_inventory_to_json(const RdmaInventory &inventory);
    RdmaInventory rdma_inventory_from_json(const nlohmann::json &j);
} // namespace sysal::detail
