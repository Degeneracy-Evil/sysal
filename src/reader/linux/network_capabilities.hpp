#pragma once

#include "sysal/model/raw_store.hpp"

#include <string>

namespace sysal::reader
{
    void read_network_capabilities(RawStore &raw, const std::string &interface);
}
