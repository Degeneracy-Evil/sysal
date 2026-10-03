#pragma once
#include "sysal/core/collect.hpp"
#include "sysal/model/hardware_health.hpp"
#include "sysal/model/raw_store.hpp"
#include "sysal/model/storage.hpp"
namespace sysal::detail
{
    void storage_findings(HardwareHealth &health, const Storage &storage, Collect flags, const RawStore &raw);
}
