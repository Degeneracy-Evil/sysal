#pragma once
#include "sysal/model/raw_store.hpp"
#include "sysal/model/storage.hpp"
namespace sysal::detail
{
    void apply_filesystems(Storage &storage, const RawStore &raw, std::vector<std::string> &warnings);
}
