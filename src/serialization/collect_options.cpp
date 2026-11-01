#include "serialization/collect_options.hpp"
#include "serialization/json_values.hpp"

namespace sysal::detail
{
    nlohmann::json collection_request_to_json(const CollectionRequest &request)
    {
        return {{"flags", static_cast<std::uint32_t>(request.flags)},
                {"options",
                 {{"filesystems", static_cast<unsigned>(request.options.filesystems)},
                  {"sensors", static_cast<unsigned>(request.options.sensors)}}}};
    }

    CollectionRequest collection_request_from_json(const nlohmann::json &json)
    {
        const auto flags = uint32_from_json(json.at("flags"), "collection request flags");
        if((flags & ~static_cast<std::uint32_t>(full)) != 0)
            throw SysalError(ErrorKind::DeserializationError, "unknown acquisition flags");
        const auto &options = json.at("options");
        return {static_cast<Collect>(flags),
                {validate_enum(uint32_from_json(options.at("filesystems"), "filesystem query policy"),
                               FilesystemQuery::AllMounted, "filesystem query policy"),
                 validate_enum(uint32_from_json(options.at("sensors"), "sensor device policy"),
                               SensorDevices::ExcludeDisplay, "sensor device policy")}};
    }
} // namespace sysal::detail
