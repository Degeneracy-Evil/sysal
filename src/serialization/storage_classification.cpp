#include "serialization/storage_classification.hpp"

#include "serialization/json_values.hpp"

#include <utility>

namespace sysal::detail
{
    nlohmann::json storage_classification_to_json(const StorageClassification &classification)
    {
        auto transports = nlohmann::json::array();
        for(const auto transport : classification.transports)
            transports.push_back(static_cast<unsigned>(transport));
        return {{"layer", static_cast<unsigned>(classification.layer)},
                {"purpose", static_cast<unsigned>(classification.purpose)},
                {"protocol", static_cast<unsigned>(classification.protocol)},
                {"transports", std::move(transports)}};
    }

    StorageClassification storage_classification_from_json(const nlohmann::json &json)
    {
        StorageClassification classification;
        classification.layer =
            validate_enum(uint32_from_json(json.at("layer"), "block layer"), BlockDeviceLayer::Virtual, "block layer");
        classification.purpose = validate_enum(uint32_from_json(json.at("purpose"), "storage purpose"),
                                               StoragePurpose::Other, "storage purpose");
        classification.protocol = validate_enum(uint32_from_json(json.at("protocol"), "storage protocol"),
                                                StorageProtocol::Scsi, "storage protocol");
        for(const auto &transport : json.at("transports").get_ref<const nlohmann::json::array_t &>())
            classification.transports.push_back(validate_enum(uint32_from_json(transport, "storage transport"),
                                                              StorageTransport::ParallelScsi, "storage transport"));
        return classification;
    }
} // namespace sysal::detail
