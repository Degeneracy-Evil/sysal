#include "parser/storage_health.hpp"
#include "parser/parse_utils.hpp"
#include "serialization/storage_health_values.hpp"

#include <map>
#include <nlohmann/json.hpp>

namespace sysal::detail
{
    namespace
    {
        using json = nlohmann::json;
        const json empty = json::object();
        const json &object(const json &j, const char *key)
        {
            const auto it = j.find(key);
            return it != j.end() && it->is_object() ? *it : empty;
        }
        std::optional<bool> boolean(const json &j, const char *key)
        {
            return j.contains(key) && j.at(key).is_boolean() ? std::optional{j.at(key).get<bool>()} : std::nullopt;
        }
        std::string text(const json &j, const char *key)
        {
            return j.contains(key) && j.at(key).is_string() ? j.at(key).get<std::string>() : "";
        }
        NvmeHealth nvme_health(const json &j, bool kelvin)
        {
            NvmeHealth h;
            const std::pair<const char *, std::optional<std::uint8_t> *> fields[] = {
                {"critical_warning", &h.critical_warning},
                {"available_spare", &h.available_spare},
                {"available_spare_threshold", &h.spare_threshold},
                {"percentage_used", &h.percentage_used}};
            for(const auto &[key, destination] : fields)
                if(j.contains(key))
                    *destination = health_integer<std::uint8_t>(j.at(key));
            // Older nvme-cli uses shorter field names.
            if(!h.available_spare && j.contains("avail_spare"))
                h.available_spare = health_integer<std::uint8_t>(j.at("avail_spare"));
            if(h.available_spare && *h.available_spare > 100)
                h.available_spare.reset();
            if(!h.percentage_used && j.contains("percent_used"))
                h.percentage_used = health_integer<std::uint8_t>(j.at("percent_used"));
            if(!h.spare_threshold && j.contains("spare_thresh"))
                h.spare_threshold = health_integer<std::uint8_t>(j.at("spare_thresh"));
            if(h.spare_threshold && *h.spare_threshold > 100)
                h.spare_threshold.reset();
            if(j.contains("temperature") && j.at("temperature").is_number_integer())
            {
                const auto &temperature = j.at("temperature");
                if(!temperature.is_number_unsigned() || temperature.get<std::uint64_t>() <= 65535)
                {
                    const auto value = temperature.get<std::int64_t>();
                    if(value >= (kelvin ? 1 : -273) && value <= 65535)
                        h.temperature = SensorTemperature{value * 1000 - (kelvin ? 273150 : 0)};
                }
            }
            const std::pair<const char *, std::optional<StorageCounter> *> counters[] = {
                {"power_on_hours", &h.power_on_hours},         {"power_cycles", &h.power_cycles},
                {"unsafe_shutdowns", &h.unsafe_shutdowns},     {"media_errors", &h.media_errors},
                {"num_err_log_entries", &h.error_log_entries}, {"data_units_read", &h.data_units_read},
                {"data_units_written", &h.data_units_written}};
            for(const auto &[key, destination] : counters)
                *destination = health_counter(j, key);
            return h;
        }
        void smart_health(StorageHealthReport &report, const json &j)
        {
            const auto &support = object(j, "smart_support");
            report.smart_available = boolean(support, "available");
            report.smart_enabled = boolean(support, "enabled");
            report.passed = boolean(object(j, "smart_status"), "passed");
            if(report.protocol == StorageProtocol::Nvme)
            {
                const auto &nvme = object(j, "nvme_smart_health_information_log");
                if(!nvme.empty())
                    report.nvme = nvme_health(nvme, false);
            }
            if(report.protocol == StorageProtocol::Ata)
            {
                const auto &attributes = object(j, "ata_smart_attributes");
                if(attributes.contains("table") && attributes.at("table").is_array())
                    for(const auto &attribute : attributes.at("table"))
                    {
                        if(!attribute.is_object() || !attribute.contains("id"))
                            continue;
                        const auto id = health_integer<std::uint8_t>(attribute.at("id"));
                        if(!id || *id == 0)
                            continue;
                        AtaHealthAttribute a;
                        a.id = *id;
                        a.name = text(attribute, "name");
                        a.raw = text(object(attribute, "raw"), "string");
                        a.when_failed = text(attribute, "when_failed");
                        const std::pair<const char *, std::optional<std::uint8_t> *> fields[] = {
                            {"value", &a.value}, {"worst", &a.worst}, {"thresh", &a.threshold}};
                        for(const auto &[key, destination] : fields)
                            if(attribute.contains(key))
                                *destination = health_integer<std::uint8_t>(attribute.at(key));
                        report.ata_attributes.push_back(std::move(a));
                    }
            }
            if(report.protocol == StorageProtocol::Scsi)
            {
                const auto &errors = object(j, "scsi_error_counter_log");
                if(!errors.empty())
                    report.scsi = ScsiHealth{health_counter(object(errors, "read"), "total_uncorrected_errors"),
                                             health_counter(object(errors, "write"), "total_uncorrected_errors"),
                                             health_counter(object(errors, "verify"), "total_uncorrected_errors")};
            }
        }
    } // namespace
    void parse_storage_health(Storage &storage, const RawStore &raw, std::vector<std::string> &warnings)
    {
        std::map<std::string, std::vector<DeviceName>> devices;
        std::map<std::string, StorageProtocol> protocols;
        for(const auto *record : raw.get_all(RawSource::StorageHealthSysfs))
        {
            if(record->status != CollectStatus::Success)
                continue;
            const auto &path = record->path_or_command;
            const auto parts = split(path, '/');
            if(path.starts_with("/sys/class/block/") && path.ends_with("/health_target") && parts.size() == 6)
                devices[trim(record->payload)].push_back(DeviceName{parts[4]});
            if(path.starts_with("/dev/") && path.ends_with("/protocol") && parts.size() == 4)
            {
                const auto protocol = trim(record->payload);
                protocols[parts[2]] = protocol == "nvme"   ? StorageProtocol::Nvme
                                      : protocol == "ata"  ? StorageProtocol::Ata
                                      : protocol == "scsi" ? StorageProtocol::Scsi
                                                           : StorageProtocol::Unknown;
            }
        }
        std::map<std::string, StorageHealthReport> reports;
        for(const auto &record : raw.records)
        {
            if(record.source != RawSource::Smartctl && record.source != RawSource::NvmeSmartLog)
                continue;
            const auto slash = record.path_or_command.find('/');
            if(slash == std::string::npos)
                continue;
            const auto target = record.path_or_command.substr(slash + 1);
            if(target.empty() || target.find('/') != std::string::npos || !protocols.contains(target))
                continue;
            StorageHealthReport report;
            report.target = DeviceName{target};
            report.devices = devices[target];
            report.protocol = protocols[target];
            report.source = record.source;
            report.origin = record.path_or_command;
            report.status = record.status;
            report.failure = record.failure;
            if(record.status == CollectStatus::Success || record.status == CollectStatus::Partial)
            {
                const auto j = json::parse(record.payload, nullptr, false);
                if(!j.is_object())
                {
                    report.status = CollectStatus::Failed;
                    report.failure = ReadFailure::Unsupported;
                    warnings.push_back("storage health: invalid JSON from " + record.path_or_command);
                }
                else if(record.source == RawSource::NvmeSmartLog)
                    report.nvme = nvme_health(j, true);
                else
                    smart_health(report, j);
            }
            reports.insert_or_assign(target, std::move(report));
        }
        for(auto &[target, report] : reports)
            storage.health.push_back(std::move(report));
    }
} // namespace sysal::detail
