#pragma once

#include "sysal/types/enums.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal
{
    /// @brief Specific inventories; completeness never applies to an entire domain.
    enum class InventoryKind
    {
        CpuPackages,
        CpuCores,
        LogicalCpus,
        NumaNodes,
        CpuCaches,
        NetworkInterfaces,
        BlockDevices,
        PciDevices,
        AcceleratorDevices
    };

    enum class InventoryCompleteness
    {
        Unknown,
        Partial,
        Complete
    };

    /// @brief Enumeration evidence, independent of optional device attributes.
    struct InventoryEnumeration
    {
        InventoryKind inventory;
        RawSource source;
        std::string origin; ///< Source/namespace scope, not a physical-host guarantee.
        std::vector<std::string> entries;
        CollectStatus status;
        std::optional<ReadFailure> failure{};
    };

    /// @brief Completeness of the normalized inventory within the stated scope.
    struct InventoryReport
    {
        InventoryKind inventory;
        InventoryCompleteness completeness{InventoryCompleteness::Unknown};
        std::string origin;
    };
} // namespace sysal
