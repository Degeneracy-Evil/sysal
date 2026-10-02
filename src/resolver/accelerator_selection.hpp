#pragma once

#include "parser/accelerator_evidence.hpp"
#include "sysal/model/accelerator.hpp"
#include "sysal/model/execution.hpp"

namespace sysal::detail
{
    using DeviceIndices = std::vector<std::size_t>;

    // nullopt: no applicable environment filter. Empty vector: an explicit empty selection.
    std::optional<DeviceIndices> select_accelerator_environment(const Accelerators &devices, AcceleratorVendor vendor,
                                                                const DeviceIndices &candidates,
                                                                const ExecutionContext &execution, bool runtime_known,
                                                                std::vector<std::string> &warnings);
} // namespace sysal::detail
