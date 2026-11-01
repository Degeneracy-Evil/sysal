#pragma once

#include "sysal/core/collect.hpp"

namespace sysal
{
    enum class FilesystemQuery
    {
        None,
        Local,
        AllMounted
    };

    enum class SensorDevices
    {
        All,
        ExcludeDisplay
    };

    struct CollectOptions
    {
        FilesystemQuery filesystems{FilesystemQuery::Local};
        SensorDevices sensors{SensorDevices::All};
        void validate() const;
    };

    struct CollectionRequest
    {
        Collect flags{full};
        CollectOptions options{};
        void validate() const;
    };
} // namespace sysal
