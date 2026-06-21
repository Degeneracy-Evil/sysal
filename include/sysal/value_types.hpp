#pragma once

#include <cstdint>
#include <string>

namespace sysal
{

struct PciAddress
{
    std::uint16_t domain{};
    std::uint8_t bus{};
    std::uint8_t device{};
    std::uint8_t function{};

    bool operator==(const PciAddress& other) const
    {
        return domain == other.domain && bus == other.bus && device == other.device &&
               function == other.function;
    }
};

template <typename Tag> struct NamedString
{
    std::string value;

    bool operator==(const NamedString& other) const { return value == other.value; }
};

struct VendorTag
{
};
struct DeviceNameTag
{
};
struct InterfaceNameTag
{
};
struct MacAddressTag
{
};
struct IpAddressTag
{
};

using Vendor = NamedString<VendorTag>;
using DeviceName = NamedString<DeviceNameTag>;
using InterfaceName = NamedString<InterfaceNameTag>;
using MacAddress = NamedString<MacAddressTag>;
using IpAddress = NamedString<IpAddressTag>;

} // namespace sysal
