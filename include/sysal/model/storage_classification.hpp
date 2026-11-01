#pragma once

#include "sysal/types/enums.hpp"

#include <vector>

namespace sysal
{
    /// @brief Kernel block layer; Device does not imply a physical drive.
    enum class BlockDeviceLayer
    {
        Unknown,
        Device,
        Partition,
        DeviceMapper,
        Raid,
        Loop,
        Virtual
    };

    enum class StoragePurpose
    {
        Unknown,
        Disk,
        Optical,
        Other
    };

    enum class StorageTransport
    {
        Unknown,
        Pcie,
        Sata,
        Sas,
        Usb,
        Tcp,
        Rdma,
        FibreChannel,
        Iscsi,
        ParallelScsi
    };

    struct StorageClassification
    {
        BlockDeviceLayer layer{BlockDeviceLayer::Unknown};
        StoragePurpose purpose{StoragePurpose::Unknown};
        StorageProtocol protocol{StorageProtocol::Unknown};
        /// @brief Distinct explicitly reported transports; not a complete path inventory.
        std::vector<StorageTransport> transports{};
    };
} // namespace sysal
