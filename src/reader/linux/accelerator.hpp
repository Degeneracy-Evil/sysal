#pragma once

#include "sysal/model/raw_store.hpp"

namespace sysal::reader
{
    /// Collect optional NVML and DRM evidence; no backend dependency is required to load Sysal.
    void read_accelerator_backends(RawStore &raw);
} // namespace sysal::reader
