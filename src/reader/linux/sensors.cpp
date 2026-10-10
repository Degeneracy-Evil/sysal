#include "reader/linux/sensors.hpp"
#include "parser/parse_utils.hpp"
#include "reader/linux/file_utils.hpp"

#include <algorithm>
#include <filesystem>
#include <string_view>

namespace sysal::reader
{
    namespace
    {
        namespace fs = std::filesystem;
        bool channel_attribute(std::string_view name)
        {
            const auto separator = name.find('_');
            if(separator == std::string_view::npos)
                return false;
            const auto channel = name.substr(0, separator);
            const auto start = channel.starts_with("temp")    ? 4U
                               : channel.starts_with("fan")   ? 3U
                               : channel.starts_with("power") ? 5U
                                                              : 0U;
            if(start == 0 || channel.size() <= start ||
               channel.find_first_not_of("0123456789", start) != std::string_view::npos)
                return false;
            const auto field = name.substr(separator + 1);
            return field == "input" || field == "average" || field == "label" || field == "type" || field == "min" ||
                   field == "max" || field == "crit" || field == "enable" || field == "fault" || field == "alarm" ||
                   field.ends_with("_alarm");
        }
        bool display_device(const fs::path &device)
        {
            for(auto path = device; !path.empty() && path != path.root_path(); path = path.parent_path())
                if(detail::parse_pci_address(path.filename().string()))
                    if(const auto value = read_file((path / "class").string()))
                        if(const auto code = detail::parse_hex(detail::trim(*value).substr(2)))
                            return (*code >> 16) == 3;
            return false;
        }
        void scan(RawStore &raw, const fs::path &base, RawSource source)
        {
            std::error_code ec;
            const auto entries = directory_entries(base, ec);
            if(ec && entries.empty())
            {
                add_record(raw, source, base.string(), "", CollectStatus::NotCollected, file_failure(ec.value()));
                return;
            }
            bool found = false;
            for(const auto &entry : entries)
            {
                const auto name = entry.path().filename().string();
                if(source == RawSource::SysfsThermal && !name.starts_with("thermal_zone"))
                    continue;
                std::error_code link_ec;
                auto device = fs::canonical(entry.path() / "device", link_ec);
                if(link_ec)
                    device = fs::canonical(entry.path(), link_ec);
                if(!link_ec && display_device(device))
                    continue;
                found = true;
                if(!link_ec)
                    add_record(raw, source, (entry.path() / "device_path").string(), device.string(),
                               CollectStatus::Success);
                if(!link_ec)
                    for(auto path = device; !path.empty() && path != path.root_path(); path = path.parent_path())
                    {
                        std::error_code driver_ec;
                        const auto driver = fs::read_symlink(path / "driver", driver_ec);
                        if(!driver_ec)
                        {
                            add_record(raw, source, (entry.path() / "driver").string(), driver.filename().string(),
                                       CollectStatus::Success);
                            break;
                        }
                    }

                std::error_code attributes_ec;
                const auto attributes = directory_entries(entry.path(), attributes_ec);
                for(const auto &attribute : attributes)
                {
                    const auto field = attribute.path().filename().string();
                    const bool accepted = source == RawSource::SysfsHwmon
                                              ? field == "name" || channel_attribute(field)
                                              : field == "type" || field == "temp" ||
                                                    (field.starts_with("trip_point_") &&
                                                     (field.ends_with("_type") || field.ends_with("_temp")));
                    if(accepted)
                        read_file_record(raw, source, attribute.path().string());
                }
                if(attributes_ec)
                    add_record(raw, source, entry.path().string(), "", CollectStatus::Failed,
                               file_failure(attributes_ec.value()));
            }
            record_directory_failure(raw, source, base, ec);
            if(!found && !ec)
                add_record(raw, source, base.string(), "", CollectStatus::NotCollected, ReadFailure::NotPresent);
        }
    } // namespace
    void read_sensors(RawStore &raw)
    {
        scan(raw, "/sys/class/hwmon", RawSource::SysfsHwmon);
        scan(raw, "/sys/class/thermal", RawSource::SysfsThermal);
    }
} // namespace sysal::reader
