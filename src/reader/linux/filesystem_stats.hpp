#pragma once

#include "sysal/model/storage.hpp"

#include <sys/statvfs.h>

namespace sysal::reader
{
    struct FilesystemStatistics
    {
        struct statvfs values
        {
        };
        std::optional<ReadFailure> failure;
        std::optional<SkipReason> skip;
        std::string note;
    };

    FilesystemStatistics read_filesystem_statistics(const StorageMount &mount);
} // namespace sysal::reader
