#pragma once
#include "sysal/core/collect_options.hpp"
#include "sysal/model/raw_store.hpp"

namespace sysal::reader
{
    void read_filesystems(RawStore &raw, FilesystemQuery policy = FilesystemQuery::Local);
}
