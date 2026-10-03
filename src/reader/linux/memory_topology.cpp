#include "reader/linux/memory_topology.hpp"

#include "reader/linux/file_utils.hpp"

#include <filesystem>
#include <string_view>

namespace sysal::reader
{
    namespace
    {
        namespace fs = std::filesystem;

        bool numbered(std::string_view name, std::string_view prefix)
        {
            return name.starts_with(prefix) && name.size() > prefix.size() &&
                   name.find_first_not_of("0123456789", prefix.size()) == std::string_view::npos;
        }

        void read_devices(RawStore &raw, const fs::path &controller)
        {
            std::error_code ec;
            for(const auto &entry : fs::directory_iterator(controller, ec))
            {
                const auto name = entry.path().filename().string();
                if((!numbered(name, "dimm") && !numbered(name, "rank")) || !entry.is_directory(ec))
                    continue;
                for(const auto *field :
                    {"dimm_mem_type", "size", "dimm_label", "dimm_location", "dimm_dev_type", "dimm_edac_mode"})
                    read_file_record(raw, RawSource::SysfsEdac, (entry.path() / field).string());
            }
            if(ec)
                add_record(raw, RawSource::SysfsEdac, controller.string(), "", CollectStatus::Failed,
                           file_failure(ec.value()));
        }
    } // namespace

    void read_edac_sysfs(RawStore &raw)
    {
        const fs::path root = "/sys/devices/system/edac/mc";
        std::error_code ec;
        if(!fs::is_directory(root, ec))
        {
            add_record(raw, RawSource::SysfsEdac, root.string(), "", CollectStatus::NotCollected,
                       ec ? file_failure(ec.value()) : ReadFailure::NotPresent);
            return;
        }
        bool found = false;
        for(const auto &entry : fs::directory_iterator(root, ec))
        {
            if(!numbered(entry.path().filename().string(), "mc") || !entry.is_directory(ec))
                continue;
            found = true;
            for(const auto *field : {"mc_name", "size_mb", "ce_count", "ue_count", "device/numa_node", "max_location"})
                read_file_record(raw, RawSource::SysfsEdac, (entry.path() / field).string());
            std::error_code device_ec;
            const auto device = fs::read_symlink(entry.path() / "device", device_ec);
            if(!device_ec)
                add_record(raw, RawSource::SysfsEdac, (entry.path() / "device").string(), device.string(),
                           CollectStatus::Success);
            read_devices(raw, entry.path());
        }
        if(ec || !found)
            add_record(raw, RawSource::SysfsEdac, root.string(), "", CollectStatus::NotCollected,
                       ec ? file_failure(ec.value()) : ReadFailure::NotPresent);
    }
} // namespace sysal::reader
