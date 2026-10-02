#pragma once

#include "parser/accelerator_evidence.hpp"

#include "sysal/model/accelerator.hpp"
#include "sysal/model/execution.hpp"

namespace sysal::detail
{
    void resolve_accelerator_visibility(Accelerators &devices, ExecutionContext &execution,
                                        std::vector<std::string> &warnings,
                                        const std::vector<RuntimeVisibility> &runtime = {});
}
