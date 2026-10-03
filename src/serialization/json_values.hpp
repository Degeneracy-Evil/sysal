#pragma once

#include "sysal/core/error.hpp"
#include "sysal/types/value_types.hpp"

#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace sysal::detail
{
    inline std::uint64_t checked_unsigned(const nlohmann::json &j, std::uint64_t maximum, std::string_view field)
    {
        if(!j.is_number_integer() || (!j.is_number_unsigned() && j.get<std::int64_t>() < 0) ||
           j.get<std::uint64_t>() > maximum)
            throw SysalError(ErrorKind::DeserializationError, "无效无符号整数: " + std::string(field));
        return j.get<std::uint64_t>();
    }

    inline std::uint32_t uint32_from_json(const nlohmann::json &j, std::string_view field)
    {
        return static_cast<std::uint32_t>(checked_unsigned(j, std::numeric_limits<std::uint32_t>::max(), field));
    }

    inline std::uint64_t uint64_from_json(const nlohmann::json &j, std::string_view field)
    {
        return checked_unsigned(j, std::numeric_limits<std::uint64_t>::max(), field);
    }

    /// @brief 从 JSON 数组读取字符串列表
    /// @param arr JSON 数组
    /// @return 字符串向量
    inline std::vector<std::string> str_array_from_json(const nlohmann::json &arr)
    {
        std::vector<std::string> result;
        for(const auto &elem : arr)
        {
            result.push_back(elem.get<std::string>());
        }
        return result;
    }

    /// @brief 校验枚举值是否在合法范围内
    /// @param val 从 JSON 读取的整数值
    /// @param max_val 枚举最大合法值
    /// @param field_name 字段名（用于错误信息）
    /// @return 校验通过的枚举值
    /// @throws SysalError 若值越界
    template <typename Enum> Enum validate_enum(std::uint32_t val, Enum max_val, std::string_view field_name)
    {
        if(val > static_cast<std::uint32_t>(max_val))
        {
            throw SysalError(ErrorKind::DeserializationError,
                             "枚举值越界: " + std::string(field_name) + " = " + std::to_string(val));
        }
        return static_cast<Enum>(val);
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
