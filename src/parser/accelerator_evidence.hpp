#pragma once

#include "sysal/types/ids.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace sysal::detail
{
    enum class AcceleratorVendor
    {
        Nvidia,
        Amd,
        Intel
    };

    constexpr std::string_view vendor_name(AcceleratorVendor vendor)
    {
        switch(vendor)
        {
        case AcceleratorVendor::Nvidia:
            return "NVIDIA";
        case AcceleratorVendor::Amd:
            return "AMD";
        case AcceleratorVendor::Intel:
            return "Intel";
        }
        return "";
    }

    constexpr std::optional<AcceleratorVendor> accelerator_vendor(std::string_view name)
    {
        if(name == "NVIDIA")
            return AcceleratorVendor::Nvidia;
        if(name == "AMD")
            return AcceleratorVendor::Amd;
        if(name == "Intel")
            return AcceleratorVendor::Intel;
        return std::nullopt;
    }

    // Presence means complete enumeration. An empty set explicitly means zero visible devices.
    struct RuntimeVisibility
    {
        AcceleratorVendor vendor;
        std::vector<AcceleratorId> visible;
    };
} // namespace sysal::detail
