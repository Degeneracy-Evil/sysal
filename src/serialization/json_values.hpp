#pragma once

#include "sysal/core/error.hpp"
#include "sysal/types/value_types.hpp"

#include <nlohmann/json.hpp>
#include <string_view>

namespace sysal::detail
{
    inline std::uint64_t checked_unsigned(const nlohmann::json &j, std::uint64_t maximum, std::string_view field)
    {
        if(!j.is_number_integer() || (!j.is_number_unsigned() && j.get<std::int64_t>() < 0) ||
           j.get<std::uint64_t>() > maximum)
            throw SysalError(ErrorKind::DeserializationError, "无效无符号整数: " + std::string(field));
        return j.get<std::uint64_t>();
    }

    inline nlohmann::json pci_address_to_json(const PciAddress &address)
    {
        return {{"domain", address.domain},
                {"bus", address.bus},
                {"device", address.device},
                {"function", address.function}};
    }

    inline PciAddress pci_address_from_json(const nlohmann::json &j)
    {
        return {static_cast<std::uint16_t>(checked_unsigned(j.at("domain"), 0xffff, "PCI domain")),
                static_cast<std::uint8_t>(checked_unsigned(j.at("bus"), 0xff, "PCI bus")),
                static_cast<std::uint8_t>(checked_unsigned(j.at("device"), 0x1f, "PCI device")),
                static_cast<std::uint8_t>(checked_unsigned(j.at("function"), 7, "PCI function"))};
    }
} // namespace sysal::detail
