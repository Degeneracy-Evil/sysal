#include "reader/linux/pci.hpp"

#include "reader/linux/file_utils.hpp"

#include <filesystem>
namespace sysal::reader
{
    namespace
    {
        namespace fs = std::filesystem;

        void read_link(RawStore &raw, const fs::path &path)
        {
            std::error_code error;
            const auto target = fs::read_symlink(path, error);
            add_record(raw, RawSource::SysfsPci, path.string(), target.string(),
                       error ? CollectStatus::Failed : CollectStatus::Success,
                       error ? std::optional{file_failure(error.value())} : std::nullopt);
        }
    } // namespace

    void read_pci_sysfs(RawStore &raw)
    {
        const fs::path base = "/sys/bus/pci/devices";
        std::error_code error;
        fs::directory_iterator entry(base, error);
        if(error)
        {
            add_record(raw, RawSource::SysfsPci, base.string(), "", CollectStatus::Failed, file_failure(error.value()));
            return;
        }
        bool found = false;
        const fs::directory_iterator end;
        for(; entry != end && !error; entry.increment(error))
        {
            const auto &directory = entry->path();
            found = true;
            for(const auto *field : {"vendor", "device", "class", "numa_node", "physical_slot", "label",
                                     "current_link_speed", "max_link_speed", "current_link_width", "max_link_width",
                                     "local_cpulist", "sriov_totalvfs", "sriov_numvfs"})
                read_file_record(raw, RawSource::SysfsPci, (directory / field).string());
            for(const auto *link : {"driver", "physfn"})
                read_link(raw, directory / link);

            std::error_code path_error;
            const auto path = fs::canonical(directory, path_error);
            add_record(raw, RawSource::SysfsPci, (directory / "sysfs_path").string(), path.string(),
                       path_error ? CollectStatus::Failed : CollectStatus::Success,
                       path_error ? std::optional{file_failure(path_error.value())} : std::nullopt);
        }
        if(error)
            add_record(raw, RawSource::SysfsPci, base.string(), "", CollectStatus::Partial,
                       file_failure(error.value()));
        else if(!found)
            add_record(raw, RawSource::SysfsPci, base.string(), "", CollectStatus::Failed, ReadFailure::NotPresent);
    }
} // namespace sysal::reader
