#pragma once

#include "sysal/model/pci.hpp"
#include <nlohmann/json.hpp>

namespace sysal::detail
{
    nlohmann::json pci_to_json(const Pci &value);
    Pci pci_from_json(const nlohmann::json &json);
} // namespace sysal::detail
