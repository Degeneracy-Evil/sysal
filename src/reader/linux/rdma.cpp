#include "reader/linux/rdma.hpp"

#include "reader/linux/file_utils.hpp"

#include <filesystem>

namespace sysal::reader
{
    namespace
    {
        namespace fs = std::filesystem;

        bool numeric_name(const fs::path &path)
        {
            const auto name = path.filename().string();
            return !name.empty() && name.find_first_not_of("0123456789") == std::string::npos;
        }

        void directory_status(RawStore &raw, const fs::path &path, const std::error_code &error)
        {
            const auto failure = error ? std::optional{file_failure(error.value())} : std::nullopt;
            add_record(raw, RawSource::SysfsRdma, path.string(), "",
                       error ? CollectStatus::NotCollected : CollectStatus::Success, failure);
        }

        void read_port(RawStore &raw, const fs::path &path)
        {
            add_record(raw, RawSource::SysfsRdma, path.string(), "", CollectStatus::Success);
            for(const auto *field :
                {"state", "phys_state", "rate", "link_layer", "lid", "sm_lid", "sm_sl", "lid_mask_count", "cap_mask"})
                read_file_record(raw, RawSource::SysfsRdma, (path / field).string());
            const auto ndevs = path / "gid_attrs" / "ndevs";
            std::error_code error;
            fs::directory_iterator entry(ndevs, error);
            const fs::directory_iterator end;
            for(; entry != end && !error; entry.increment(error))
                if(numeric_name(entry->path()))
                    read_file_record(raw, RawSource::SysfsRdma, entry->path().string());
            directory_status(raw, ndevs, error);
        }

        void read_device(RawStore &raw, const fs::path &path)
        {
            add_record(raw, RawSource::SysfsRdma, path.string(), "", CollectStatus::Success);
            for(const auto *field : {"node_type", "node_guid", "sys_image_guid", "node_desc", "fw_ver"})
                read_file_record(raw, RawSource::SysfsRdma, (path / field).string());

            std::error_code error;
            const auto canonical = fs::canonical(path, error);
            add_record(raw, RawSource::SysfsRdma, (path / "sysfs_path").string(), canonical.string(),
                       error ? CollectStatus::Failed : CollectStatus::Success,
                       error ? std::optional{file_failure(error.value())} : std::nullopt);
            const auto driver = fs::read_symlink(path / "device" / "driver", error);
            add_record(raw, RawSource::SysfsRdma, (path / "device" / "driver").string(), driver.string(),
                       error ? CollectStatus::Failed : CollectStatus::Success,
                       error ? std::optional{file_failure(error.value())} : std::nullopt);

            const auto net = path / "device" / "net";
            fs::directory_iterator interface(net, error);
            const fs::directory_iterator end;
            for(; interface != end && !error; interface.increment(error))
                add_record(raw, RawSource::SysfsRdma, interface->path().string(), "", CollectStatus::Success);
            directory_status(raw, net, error);

            const auto ports = path / "ports";
            fs::directory_iterator port(ports, error);
            for(; port != end && !error; port.increment(error))
                if(numeric_name(port->path()))
                    read_port(raw, port->path());
            directory_status(raw, ports, error);
        }
    } // namespace

    void read_rdma_sysfs(RawStore &raw)
    {
        const fs::path root = "/sys/class/infiniband";
        std::error_code error;
        fs::directory_iterator device(root, error);
        const fs::directory_iterator end;
        bool found = false;
        for(; device != end && !error; device.increment(error))
        {
            found = true;
            read_device(raw, device->path());
        }
        if(found && error)
            add_record(raw, RawSource::SysfsRdma, root.string(), "", CollectStatus::Partial,
                       file_failure(error.value()));
        else
            directory_status(raw, root, error);
    }
} // namespace sysal::reader
