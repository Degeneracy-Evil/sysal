#pragma once

#include "sysal/model/network.hpp"
#include "sysal/model/raw_store.hpp"

namespace sysal::detail
{
    void apply_network_details(Network &network, const RawStore &raw);
}
