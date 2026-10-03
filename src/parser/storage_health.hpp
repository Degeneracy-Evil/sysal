#pragma once
#include "sysal/model/raw_store.hpp"
#include "sysal/model/storage.hpp"
namespace sysal::detail
{
    void parse_storage_health(Storage &storage, const RawStore &raw, std::vector<std::string> &warnings);
}
