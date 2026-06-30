/// @file sysal.hpp
/// @brief sysal 公共 API 总入口
/// @details 包含 sysal 库的全部公共头文件，调用方只需包含此单一头文件。

#pragma once

#include "sysal/core/collect.hpp"
#include "sysal/core/error.hpp"
#include "sysal/core/system.hpp"
#include "sysal/model/accelerator.hpp"
#include "sysal/model/cpu.hpp"
#include "sysal/model/execution.hpp"
#include "sysal/model/memory.hpp"
#include "sysal/model/network.hpp"
#include "sysal/model/pci.hpp"
#include "sysal/model/platform.hpp"
#include "sysal/model/raw_store.hpp"
#include "sysal/model/snapshot_meta.hpp"
#include "sysal/model/software.hpp"
#include "sysal/types/enums.hpp"
#include "sysal/types/ids.hpp"
#include "sysal/types/strong_id.hpp"
#include "sysal/types/units.hpp"
#include "sysal/types/value_types.hpp"
