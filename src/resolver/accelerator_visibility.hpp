#pragma once

#include "sysal/model/accelerator.hpp"
#include "sysal/model/execution.hpp"

namespace sysal::detail
{
    void resolve_accelerator_visibility(Accelerators &devices, ExecutionContext &execution,
                                        std::vector<std::string> &warnings,
                                        const std::vector<std::pair<AcceleratorId, bool>> &runtime = {});
}
