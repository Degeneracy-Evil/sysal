#pragma once

#include "sysal/types/enums.hpp"
#include "sysal/types/units.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sysal
{
    enum class FilesystemKind
    {
        Local,
        Network,
        Memory,
        Pseudo,
        Userspace,
        Unknown
    };

    /// Filesystem statistics are a view, not additive physical storage capacity.
    struct FilesystemUsage
    {
        MemorySize total;
        std::optional<MemorySize> free;
        std::optional<MemorySize> available; ///< Excludes reserved blocks; not a per-user quota guarantee.
        std::optional<std::uint64_t> inodes;
        std::optional<std::uint64_t> free_inodes;
        std::optional<std::uint64_t> available_inodes;
        std::string filesystem_id;
    };

    struct FilesystemReport
    {
        std::vector<std::uint32_t> mount_ids; ///< Aliases of the same mountinfo filesystem view.
        FilesystemKind kind{FilesystemKind::Unknown};
        CollectStatus status{CollectStatus::NotCollected};
        std::optional<ReadFailure> failure;
        std::string note;
        std::optional<FilesystemUsage> usage;
    };
} // namespace sysal
