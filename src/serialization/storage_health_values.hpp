#pragma once
#include "sysal/model/storage_health.hpp"
#include <limits>
#include <nlohmann/json.hpp>
#include <string_view>

namespace sysal::detail
{
    template <typename Integer> std::optional<Integer> health_integer(const nlohmann::json &j)
    {
        if(!j.is_number_integer() || (!j.is_number_unsigned() && j.get<std::int64_t>() < 0))
            return std::nullopt;
        const auto value = j.get<std::uint64_t>();
        if(value > std::numeric_limits<Integer>::max())
            return std::nullopt;
        return static_cast<Integer>(value);
    }
    inline std::optional<StorageCounter> storage_counter(const nlohmann::json &j)
    {
        std::string value;
        if(j.is_string())
            value = j.get<std::string>();
        else if(auto number = health_integer<std::uint64_t>(j))
            value = std::to_string(*number);
        else
            return std::nullopt;
        if(value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
            return std::nullopt;
        const auto first = value.find_first_not_of('0');
        value = first == std::string::npos ? "0" : value.substr(first);
        constexpr std::string_view maximum = "340282366920938463463374607431768211455";
        if(value.size() > maximum.size() || (value.size() == maximum.size() && value > maximum))
            return std::nullopt;
        return StorageCounter{value};
    }
    inline std::optional<StorageCounter> health_counter(const nlohmann::json &j, const std::string &key)
    {
        // smartctl -jv emits exact decimal companions even when numeric JSON exceeds 64 bits.
        if(j.contains(key + "_s"))
            return storage_counter(j.at(key + "_s"));
        return j.contains(key) ? storage_counter(j.at(key)) : std::nullopt;
    }
} // namespace sysal::detail
