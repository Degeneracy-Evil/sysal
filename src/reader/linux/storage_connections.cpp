#include "reader/linux/storage_connections.hpp"

#include "reader/linux/file_utils.hpp"

#include <filesystem>
#include <string_view>

namespace sysal::reader
{
    namespace
    {
        namespace fs = std::filesystem;
        constexpr auto source = RawSource::SysfsStorageConnections;

        void canonical_record(RawStore &raw, const fs::path &origin, const fs::path &target)
        {
            std::error_code ec;
            const auto path = fs::canonical(target, ec);
            add_record(raw, source, origin.string(), path.string(), ec ? CollectStatus::Failed : CollectStatus::Success,
                       ec ? std::optional{file_failure(ec.value())} : std::nullopt);
        }

        template <typename Read> void entries(RawStore &raw, const fs::path &root, Read read)
        {
            std::error_code ec;
            fs::directory_iterator entry(root, ec);
            if(ec)
            {
                add_record(raw, source, root.string(), "", CollectStatus::NotCollected, file_failure(ec.value()));
                return;
            }
            add_record(raw, source, root.string(), "", CollectStatus::Success);
            const fs::directory_iterator end;
            for(; entry != end && !ec; entry.increment(ec))
                read(entry->path());
            if(ec)
                add_record(raw, source, root.string(), "", CollectStatus::Partial, file_failure(ec.value()));
        }

        bool numbered(std::string_view name, std::string_view prefix)
        {
            return name.starts_with(prefix) && name.size() > prefix.size() &&
                   name.find_first_not_of("0123456789", prefix.size()) == std::string_view::npos;
        }

        void nvme_controller(RawStore &raw, const fs::path &path)
        {
            if(!numbered(path.filename().string(), "nvme"))
                return;
            canonical_record(raw, path / "sysfs_path", path);
            for(const auto *field :
                {"transport", "address", "model", "serial", "firmware_rev", "state", "cntlid", "subsysnqn"})
                read_file_record(raw, source, (path / field).string());
        }

        void scsi_host(RawStore &raw, const fs::path &path)
        {
            if(!numbered(path.filename().string(), "host"))
                return;
            canonical_record(raw, path / "sysfs_path", path);
            for(const auto *field : {"proc_name", "state", "supported_mode", "active_mode"})
                read_file_record(raw, source, (path / field).string());
        }

        void block_connection(RawStore &raw, const fs::path &path)
        {
            std::error_code ec;
            if(fs::exists(path / "partition", ec) || ec)
                return;
            if(!fs::exists(path / "device", ec) || ec)
                return; // Virtual block layers have no protocol device.
            canonical_record(raw, path / "device_path", path / "device");
            if(fs::exists(path / "nsid", ec) && !ec)
            {
                for(const auto *field : {"nsid", "nguid", "eui"})
                    read_file_record(raw, source, (path / field).string());
                if(fs::is_directory(path / "multipath", ec) && !ec)
                    entries(raw, path / "multipath",
                            [&](const auto &entry)
                            {
                                std::error_code link_ec;
                                if(fs::is_symlink(entry, link_ec) && !link_ec)
                                    canonical_record(raw, entry, entry);
                            });
            }
            else if(fs::exists(path / "device/type", ec) && !ec)
                for(const auto *field : {"device/type", "device/state"})
                    read_file_record(raw, source, (path / field).string());
        }
    } // namespace

    void read_storage_connections(RawStore &raw)
    {
        entries(raw, "/sys/class/nvme", [&](const auto &path) { nvme_controller(raw, path); });
        entries(raw, "/sys/class/scsi_host", [&](const auto &path) { scsi_host(raw, path); });
        entries(raw, "/sys/class/block", [&](const auto &path) { block_connection(raw, path); });
    }
} // namespace sysal::reader
