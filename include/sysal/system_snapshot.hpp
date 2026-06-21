#pragma once

#include "sysal/diagnostics.hpp"
#include "sysal/execution_context_info.hpp"
#include "sysal/platform_info.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/resource_info.hpp"
#include "sysal/snapshot_meta.hpp"
#include "sysal/software_stack_info.hpp"

#include <optional>

namespace sysal
{

struct SystemSnapshot
{
    SnapshotMeta meta;
    PlatformInfo platform;
    ResourceInfo resources;
    SoftwareStackInfo software;
    ExecutionContextInfo execution;
    Diagnostics diagnostics;
    std::optional<RawStore> raw;
};

} // namespace sysal
