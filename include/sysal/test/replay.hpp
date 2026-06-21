#pragma once

#include "sysal/collect_spec.hpp"
#include "sysal/error.hpp"
#include "sysal/expected.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/system_snapshot.hpp"

#include <string>

namespace sysal::test
{

Expected<RawStore, SysalError> load_raw_store(const std::string& path);

Expected<SystemSnapshot, SysalError> collect_from_raw(const RawStore& raw,
                                                      const CollectSpec& spec = {});

Expected<void, SysalError> save_raw_store(const RawStore& raw, const std::string& path);

} // namespace sysal::test
