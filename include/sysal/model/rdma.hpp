#pragma once

#include "sysal/types/enums.hpp"
#include "sysal/types/ids.hpp"
#include "sysal/types/units.hpp"
#include "sysal/types/value_types.hpp"

#include <optional>
#include <vector>

namespace sysal
{
    struct RdmaPortNumberTag
    {
    };
    struct RdmaLidTag
    {
    };
    struct RdmaGuidTag
    {
    };
    struct RdmaDeviceNameTag
    {
    };
    using RdmaPortNumber = StrongId<std::uint32_t, RdmaPortNumberTag>;
    using RdmaLid = StrongId<std::uint32_t, RdmaLidTag>; ///< 与内核 ib_port_attr 的 LID 宽度一致
    using RdmaGuid = NamedString<RdmaGuidTag>;
    using RdmaDeviceName = NamedString<RdmaDeviceNameTag>;

    enum class RdmaNodeType
    {
        Unknown,
        ChannelAdapter,
        Switch,
        Router,
        Rnic,
        Usnic,
        UsnicUdp
    };
    enum class RdmaPortState
    {
        Unknown,
        Down,
        Init,
        Armed,
        Active,
        ActiveDeferred
    };
    enum class RdmaPhysicalState
    {
        Unknown,
        Sleep,
        Polling,
        Disabled,
        Training,
        LinkUp,
        Recovery,
        PhyTest
    };
    enum class RdmaLinkLayer
    {
        Unknown,
        InfiniBand,
        Ethernet
    };

    struct RdmaPort
    {
        RdmaPortNumber number;
        RdmaPortState state{};
        RdmaPhysicalState physical_state{};
        RdmaLinkLayer link_layer{};
        std::string rate_report{}; ///< 内核报告及单位，不代表应用吞吐量
        std::optional<Bandwidth> rate{};
        std::optional<RdmaLid> lid{};
        std::optional<RdmaLid> subnet_manager_lid{};
        std::optional<std::uint8_t> subnet_manager_sl{};
        std::optional<std::uint8_t> lid_mask_count{};
        std::optional<std::uint32_t> capability_mask{};
        std::vector<InterfaceName> network_interfaces{}; ///< 仅来自明确 GID ndev 报告
    };

    struct RdmaDevice
    {
        RdmaDeviceId id; ///< 仅在当前快照内有效
        RdmaDeviceName name;
        RdmaNodeType node_type{};
        std::string description{};
        RdmaGuid node_guid{};
        RdmaGuid system_image_guid{};
        std::string firmware_version{};
        std::string driver{};
        std::optional<PciAddress> pci_address{};
        std::optional<NumaNodeId> numa_node{};
        Vendor vendor{};
        DeviceName device_name{};
        std::vector<InterfaceName> network_interfaces{}; ///< 同一 backing device 的接口，非端口映射
        std::vector<RdmaPort> ports{};
    };

    struct RdmaInventory
    {
        CollectStatus status{CollectStatus::NotCollected}; ///< 类目录发现状态，非健康或属性读取状态
        std::optional<ReadFailure> failure{ReadFailure::NotProvided};
        std::vector<RdmaDevice> devices{};
    };
} // namespace sysal
