#pragma once

#include "sysal/types/ids.hpp"
#include "sysal/types/value_types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal
{
    using NvmeControllerName = NamedString<struct NvmeControllerNameTag>;
    using NvmeControllerId = StrongId<std::uint16_t, struct NvmeControllerIdTag>;
    using NvmeNamespaceId = StrongId<std::uint32_t, struct NvmeNamespaceIdTag>;
    using ScsiHostNumber = StrongId<std::uint32_t, struct ScsiHostNumberTag>;
    using ScsiChannelId = StrongId<std::uint32_t, struct ScsiChannelIdTag>;
    using ScsiTargetId = StrongId<std::uint32_t, struct ScsiTargetIdTag>;
    using ScsiLun = StrongId<std::uint64_t, struct ScsiLunTag>;
    using AtaPortNumber = StrongId<std::uint32_t, struct AtaPortNumberTag>;

    /// @brief PCI mass-storage 功能；一项不代表一张物理卡。
    struct StorageController
    {
        PciAddress pci_address;
        std::uint32_t class_code{}; ///< 24-bit PCI class/subclass/programming interface。
        Vendor vendor;
        DeviceName model;
        std::string driver;
        std::optional<NumaNodeId> numa_node;
    };

    /// @brief 内核 NVMe 控制器；Fabrics 控制器可能没有 PCI 地址。
    struct NvmeController
    {
        NvmeControllerName name;
        std::string transport;
        std::string address;
        std::string model;
        std::string serial;
        std::string firmware_revision;
        std::string state;
        std::optional<NvmeControllerId> controller_id;
        std::string subsystem_nqn;
        std::optional<PciAddress> pci_address;
        std::optional<NumaNodeId> numa_node;
    };

    /// @brief 内核 SCSI host；多个 host 可属于同一个 PCI 控制器。
    struct ScsiHost
    {
        ScsiHostNumber number;
        std::string proc_name;
        std::string state;
        std::string supported_mode;
        std::string active_mode;
        std::optional<PciAddress> pci_address;
        std::optional<NumaNodeId> numa_node;
        std::optional<AtaPortNumber> ata_port; ///< 内核 ATA port 编号，不是背板槽位。
    };

    struct ScsiAddress
    {
        ScsiHostNumber host;
        ScsiChannelId channel;
        ScsiTargetId target;
        ScsiLun lun;
    };

    struct ScsiDeviceInfo
    {
        ScsiAddress address;
        std::optional<std::uint32_t> peripheral_type;
        std::string state;
    };

    /// @brief namespace/controller 关系仅来自 sysfs 祖先或实际 multipath links。
    struct NvmeNamespaceInfo
    {
        NvmeNamespaceId id;
        std::string nguid;
        std::string eui;
        std::vector<NvmeControllerName> controllers;
    };
} // namespace sysal
