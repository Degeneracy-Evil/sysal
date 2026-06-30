/// @file value_types.hpp
/// @brief 带语义标签的值类型
/// @details 定义 PCI 地址以及若干带幻影标签的字符串类型，避免语义不同的
///          字符串（厂商、设备名、接口名、MAC、IP）相互误用。

#pragma once

#include <cstdint>
#include <string>

namespace sysal
{

/// @brief PCI 设备地址
struct PciAddress
{
    std::uint16_t domain{};  ///< PCI 域号
    std::uint8_t bus{};      ///< 总线号
    std::uint8_t device{};   ///< 设备号
    std::uint8_t function{}; ///< 功能号

    /// @brief 相等比较
    bool operator==(const PciAddress& other) const
    {
        return domain == other.domain && bus == other.bus && device == other.device &&
               function == other.function;
    }
};

/// @brief 带幻影标签的字符串类型模板
/// @details 不同 Tag 实例化为不同类型，用于在类型层面区分语义不同的字符串。
/// @tparam Tag 幻影标签类型
template <typename Tag> struct NamedString
{
    std::string value; ///< 原始字符串值

    /// @brief 相等比较
    bool operator==(const NamedString& other) const { return value == other.value; }
};

/// @brief 厂商标签
struct VendorTag
{
};
/// @brief 设备名标签
struct DeviceNameTag
{
};
/// @brief 网络接口名标签
struct InterfaceNameTag
{
};
/// @brief MAC 地址标签
struct MacAddressTag
{
};
/// @brief IP 地址标签
struct IpAddressTag
{
};

using Vendor = NamedString<VendorTag>;               ///< 厂商名称
using DeviceName = NamedString<DeviceNameTag>;       ///< 设备名称
using InterfaceName = NamedString<InterfaceNameTag>; ///< 网络接口名称
using MacAddress = NamedString<MacAddressTag>;       ///< MAC 地址
using IpAddress = NamedString<IpAddressTag>;         ///< IP 地址

} // namespace sysal
