#pragma once

#include "sysal/model/sensors.hpp"
#include "sysal/types/value_types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal
{
    /// Exact unsigned 128-bit count, encoded in decimal to avoid JSON floating-point loss.
    struct StorageCounter
    {
        std::string decimal;
    };
    enum class StorageProtocol
    {
        Unknown,
        Nvme,
        Ata,
        Scsi
    };
    struct NvmeHealth
    {
        std::optional<std::uint8_t> critical_warning;
        std::optional<std::uint8_t> available_spare;
        std::optional<std::uint8_t> spare_threshold;
        std::optional<std::uint8_t> percentage_used;
        std::optional<SensorTemperature> temperature;
        std::optional<StorageCounter> power_on_hours;
        std::optional<StorageCounter> power_cycles;
        std::optional<StorageCounter> unsafe_shutdowns;
        std::optional<StorageCounter> media_errors;
        std::optional<StorageCounter> error_log_entries;
        std::optional<StorageCounter> data_units_read;
        std::optional<StorageCounter> data_units_written;
    };
    struct AtaHealthAttribute
    {
        std::uint8_t id{};
        std::string name;
        std::optional<std::uint8_t> value;
        std::optional<std::uint8_t> worst;
        std::optional<std::uint8_t> threshold;
        std::string raw;
        std::string when_failed;
    };
    struct ScsiHealth
    {
        std::optional<StorageCounter> read_uncorrected;
        std::optional<StorageCounter> write_uncorrected;
        std::optional<StorageCounter> verify_uncorrected;
    };
    struct StorageHealthReport
    {
        DeviceName target; ///< Controller for NVMe, whole disk for ATA/SCSI.
        std::vector<DeviceName> devices;
        StorageProtocol protocol{StorageProtocol::Unknown};
        RawSource source{RawSource::Smartctl};
        std::string origin;
        CollectStatus status{CollectStatus::NotCollected};
        std::optional<ReadFailure> failure;
        std::optional<bool> smart_available;
        std::optional<bool> smart_enabled;
        std::optional<bool> passed;
        std::optional<NvmeHealth> nvme;
        std::vector<AtaHealthAttribute> ata_attributes;
        std::optional<ScsiHealth> scsi;
    };
} // namespace sysal
