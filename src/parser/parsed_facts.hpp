#pragma once

#include "sysal/execution_context_info.hpp"
#include "sysal/platform_info.hpp"
#include "sysal/resource_info.hpp"
#include "sysal/software_stack_info.hpp"

#include <optional>

namespace sysal::detail
{

struct ParsedFacts
{
    std::optional<PlatformInfo> platform;
    std::optional<CpuSubsystem> cpu;
    std::optional<MemorySubsystem> memory;
    std::optional<PciSubsystem> pci;
    std::optional<NetworkSubsystem> network;
    std::optional<AcceleratorSubsystem> accelerators;
    std::optional<StorageSubsystem> storage;
    std::optional<TopologyInfo> topology;
    std::optional<SoftwareStackInfo> software;
    std::optional<ExecutionContextInfo> execution;
};

} // namespace sysal::detail
