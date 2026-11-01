#include "reader/linux/filesystems.hpp"
#include "common/filesystem.hpp"
#include "common/mountinfo.hpp"
#include "reader/linux/file_utils.hpp"
#include "reader/linux/filesystem_stats.hpp"

#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>
#include <tuple>

namespace sysal::reader
{
    namespace
    {
        void query(RawStore &raw, const std::vector<const StorageMount *> &mounts, FilesystemQuery policy)
        {
            const auto selected =
                std::find_if(mounts.begin(), mounts.end(), [](const auto *mount) { return mount->path.value == "/"; });
            const auto &mount = **(selected == mounts.end() ? mounts.begin() : selected);
            const auto kind = detail::filesystem_kind(mount.filesystem.value);
            nlohmann::json payload{{"kind", static_cast<unsigned>(kind)}, {"mount_ids", nlohmann::json::array()}};
            for(const auto *alias : mounts)
                payload["mount_ids"].push_back(alias->mount_id);
            auto status = CollectStatus::NotCollected;
            std::optional<ReadFailure> failure;
            std::optional<SkipReason> skip;
            if(kind == FilesystemKind::Pseudo || mount.filesystem.value == "ramfs")
                skip = SkipReason::NotApplicable;
            else if(policy == FilesystemQuery::None)
                skip = SkipReason::NotRequested;
            else if(policy == FilesystemQuery::Local && kind != FilesystemKind::Local && kind != FilesystemKind::Memory)
                skip = SkipReason::PolicyExcluded;
            if(!skip)
            {
                const auto result = read_filesystem_statistics(mount);
                failure = result.failure;
                skip = result.skip;
                if(!result.note.empty())
                    payload["note"] = result.note;
                const auto &values = result.values;
                if(failure)
                {
                    status = CollectStatus::Failed;
                }
                else if(!skip)
                {
                    status = CollectStatus::Success;
                    payload["stats"] = {{"block_size", values.f_frsize ? values.f_frsize : values.f_bsize},
                                        {"blocks", values.f_blocks},
                                        {"free_blocks", values.f_bfree},
                                        {"available_blocks", values.f_bavail},
                                        {"inodes", values.f_files},
                                        {"free_inodes", values.f_ffree},
                                        {"available_inodes", values.f_favail},
                                        {"filesystem_id", std::to_string(values.f_fsid)}};
                }
            }
            add_record(raw, RawSource::FilesystemUsage, "statvfs/" + std::to_string(mount.mount_id), payload.dump(),
                       status, failure, skip);
        }
    } // namespace

    void read_filesystems(RawStore &raw, FilesystemQuery policy)
    {
        const auto evidence = raw.get_all(RawSource::StorageMountInfo);
        for(const auto *record : evidence)
            if(record->status == CollectStatus::Success)
            {
                std::vector<std::string> warnings;
                const auto mounts = detail::parse_mountinfo(record->payload, warnings);
                using Key = std::tuple<std::uint32_t, std::uint32_t, std::string, std::string>;
                std::map<Key, std::vector<const StorageMount *>> groups;
                for(const auto &mount : mounts)
                    groups[{mount.device_number.major, mount.device_number.minor, mount.filesystem.value, mount.source}]
                        .push_back(&mount);
                for(const auto &[key, aliases] : groups)
                {
                    (void)key;
                    query(raw, aliases, policy);
                }
                return;
            }
    }
} // namespace sysal::reader
