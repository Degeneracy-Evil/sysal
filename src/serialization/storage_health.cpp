#include "serialization/storage_health.hpp"
#include "serialization/storage_health_values.hpp"
#include "sysal/core/error.hpp"

namespace sysal::detail
{
    namespace
    {
        using json = nlohmann::json;
        template <typename Enum> Enum enumeration(const json &j, Enum maximum)
        {
            const auto value = health_integer<std::uint64_t>(j);
            if(!value || *value > static_cast<std::uint64_t>(maximum))
                throw SysalError(ErrorKind::DeserializationError, "invalid storage health enum");
            return static_cast<Enum>(*value);
        }
        template <typename Integer> void integer(const json &j, const char *key, std::optional<Integer> &destination)
        {
            if(!j.contains(key))
                return;
            destination = health_integer<Integer>(j.at(key));
            if(!destination)
                throw SysalError(ErrorKind::DeserializationError, "invalid storage health integer");
        }
        void counter(const json &j, const char *key, std::optional<StorageCounter> &destination)
        {
            if(!j.contains(key))
                return;
            destination = storage_counter(j.at(key));
            if(!destination)
                throw SysalError(ErrorKind::DeserializationError, "invalid storage health counter");
        }
        template <typename Value> void put(json &j, const char *key, const std::optional<Value> &value)
        {
            if(value)
                j[key] = *value;
        }
        void put(json &j, const char *key, const std::optional<StorageCounter> &value)
        {
            if(value)
            {
                const auto validated = storage_counter(value->decimal);
                if(!validated)
                    throw SysalError(ErrorKind::SerializationError, "invalid storage health counter");
                j[key] = validated->decimal;
            }
        }
        json nvme_to_json(const NvmeHealth &h)
        {
            json j = json::object();
            put(j, "critical_warning", h.critical_warning);
            put(j, "available_spare", h.available_spare);
            put(j, "spare_threshold", h.spare_threshold);
            put(j, "percentage_used", h.percentage_used);
            if(h.temperature)
                j["temperature"] = h.temperature->value;
            put(j, "power_on_hours", h.power_on_hours);
            put(j, "power_cycles", h.power_cycles);
            put(j, "unsafe_shutdowns", h.unsafe_shutdowns);
            put(j, "media_errors", h.media_errors);
            put(j, "error_log_entries", h.error_log_entries);
            put(j, "data_units_read", h.data_units_read);
            put(j, "data_units_written", h.data_units_written);
            return j;
        }
        NvmeHealth nvme_from_json(const json &j)
        {
            NvmeHealth h;
            integer(j, "critical_warning", h.critical_warning);
            integer(j, "available_spare", h.available_spare);
            integer(j, "spare_threshold", h.spare_threshold);
            integer(j, "percentage_used", h.percentage_used);
            if(j.contains("temperature"))
            {
                const auto &value = j.at("temperature");
                if(!value.is_number_integer() ||
                   (value.is_number_unsigned() &&
                    value.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())))
                    throw SysalError(ErrorKind::DeserializationError, "invalid storage temperature");
                h.temperature = SensorTemperature{value.get<std::int64_t>()};
            }
            counter(j, "power_on_hours", h.power_on_hours);
            counter(j, "power_cycles", h.power_cycles);
            counter(j, "unsafe_shutdowns", h.unsafe_shutdowns);
            counter(j, "media_errors", h.media_errors);
            counter(j, "error_log_entries", h.error_log_entries);
            counter(j, "data_units_read", h.data_units_read);
            counter(j, "data_units_written", h.data_units_written);
            return h;
        }
    } // namespace
    nlohmann::json storage_health_to_json(const StorageHealthReport &h)
    {
        json j{{"target", h.target.value},
               {"devices", json::array()},
               {"protocol", static_cast<unsigned>(h.protocol)},
               {"source", static_cast<unsigned>(h.source)},
               {"origin", h.origin},
               {"status", static_cast<unsigned>(h.status)}};
        for(const auto &device : h.devices)
            j["devices"].push_back(device.value);
        if(h.failure)
            j["failure"] = static_cast<unsigned>(*h.failure);
        put(j, "smart_available", h.smart_available);
        put(j, "smart_enabled", h.smart_enabled);
        put(j, "passed", h.passed);
        if(h.nvme)
            j["nvme"] = nvme_to_json(*h.nvme);
        if(!h.ata_attributes.empty())
        {
            j["ata_attributes"] = json::array();
            for(const auto &a : h.ata_attributes)
            {
                json attribute{{"id", a.id}, {"name", a.name}, {"raw", a.raw}, {"when_failed", a.when_failed}};
                put(attribute, "value", a.value);
                put(attribute, "worst", a.worst);
                put(attribute, "threshold", a.threshold);
                j["ata_attributes"].push_back(attribute);
            }
        }
        if(h.scsi)
        {
            json scsi = json::object();
            put(scsi, "read_uncorrected", h.scsi->read_uncorrected);
            put(scsi, "write_uncorrected", h.scsi->write_uncorrected);
            put(scsi, "verify_uncorrected", h.scsi->verify_uncorrected);
            j["scsi"] = scsi;
        }
        return j;
    }
    StorageHealthReport storage_health_from_json(const nlohmann::json &j)
    {
        StorageHealthReport h;
        h.target = DeviceName{j.at("target").get<std::string>()};
        for(const auto &device : j.at("devices"))
            h.devices.push_back(DeviceName{device.get<std::string>()});
        h.protocol = enumeration(j.at("protocol"), StorageProtocol::Scsi);
        h.source = enumeration(j.at("source"), RawSource::NvmeSmartLog);
        h.origin = j.at("origin").get<std::string>();
        h.status = enumeration(j.at("status"), CollectStatus::NotCollected);
        if(j.contains("failure"))
            h.failure = enumeration(j.at("failure"), ReadFailure::LowPower);
        if(j.contains("smart_available"))
            h.smart_available = j.at("smart_available").get<bool>();
        if(j.contains("smart_enabled"))
            h.smart_enabled = j.at("smart_enabled").get<bool>();
        if(j.contains("passed"))
            h.passed = j.at("passed").get<bool>();
        if(j.contains("nvme"))
            h.nvme = nvme_from_json(j.at("nvme"));
        if(j.contains("ata_attributes"))
            for(const auto &a : j.at("ata_attributes"))
            {
                const auto id = health_integer<std::uint8_t>(a.at("id"));
                if(!id || *id == 0)
                    throw SysalError(ErrorKind::DeserializationError, "invalid ATA attribute ID");
                AtaHealthAttribute attribute;
                attribute.id = *id;
                attribute.name = a.at("name").get<std::string>();
                attribute.raw = a.at("raw").get<std::string>();
                attribute.when_failed = a.at("when_failed").get<std::string>();
                integer(a, "value", attribute.value);
                integer(a, "worst", attribute.worst);
                integer(a, "threshold", attribute.threshold);
                h.ata_attributes.push_back(std::move(attribute));
            }
        if(j.contains("scsi"))
        {
            ScsiHealth scsi;
            counter(j.at("scsi"), "read_uncorrected", scsi.read_uncorrected);
            counter(j.at("scsi"), "write_uncorrected", scsi.write_uncorrected);
            counter(j.at("scsi"), "verify_uncorrected", scsi.verify_uncorrected);
            h.scsi = scsi;
        }
        return h;
    }
} // namespace sysal::detail
