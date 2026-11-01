#include "reader/linux/filesystems.hpp"
#include "common/mountinfo.hpp"
#include "reader/linux/file_utils.hpp"

#include <algorithm>
#include <cerrno>
#include <map>
#include <nlohmann/json.hpp>
#include <string_view>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <tuple>

namespace sysal::reader
{
    namespace
    {
        FilesystemKind classify(std::string_view type)
        {
            if(type == "tmpfs" || type == "devtmpfs" || type == "ramfs")
                return FilesystemKind::Memory;
            for(auto local : {"ext2", "ext3", "ext4", "xfs", "btrfs", "f2fs", "jfs", "reiserfs", "zfs", "bcachefs",
                              "vfat", "exfat", "ntfs", "ntfs3", "iso9660", "udf", "squashfs", "overlay"})
                if(type == local)
                    return FilesystemKind::Local;
            for(auto network : {"nfs", "nfs4", "cifs", "smb3", "ceph", "lustre", "beegfs", "gpfs", "afs", "9p",
                                "fuse.sshfs", "fuse.glusterfs", "fuse.ceph"})
                if(type == network)
                    return FilesystemKind::Network;
            if(type == "fuse" || type == "fuseblk" || type.starts_with("fuse."))
                return FilesystemKind::Userspace;
            for(auto pseudo : {"proc", "sysfs", "cgroup", "cgroup2", "devpts", "securityfs", "debugfs", "tracefs",
                               "pstore", "efivarfs", "configfs", "mqueue", "hugetlbfs", "binfmt_misc", "autofs",
                               "rpc_pipefs", "fusectl", "bpf", "nsfs"})
                if(type == pseudo)
                    return FilesystemKind::Pseudo;
            return FilesystemKind::Unknown;
        }

        std::optional<ReadFailure> read_statistics(const StorageMount &mount, struct statvfs &values, std::string &note)
        {
            struct stat identity
            {
            };
            if(::stat(mount.path.value.c_str(), &identity) != 0)
                return file_failure(errno);
            if(major(identity.st_dev) != mount.device_number.major ||
               minor(identity.st_dev) != mount.device_number.minor)
            {
                note = "Mount changed or is covered; statistics not associated";
                return ReadFailure::NotPresent;
            }
            if(::statvfs(mount.path.value.c_str(), &values) != 0)
                return file_failure(errno);
            return std::nullopt;
        }

        void query(RawStore &raw, const std::vector<const StorageMount *> &mounts)
        {
            const auto selected =
                std::find_if(mounts.begin(), mounts.end(), [](const auto *mount) { return mount->path.value == "/"; });
            const auto &mount = **(selected == mounts.end() ? mounts.begin() : selected);
            const auto kind = classify(mount.filesystem.value);
            nlohmann::json payload{{"kind", static_cast<unsigned>(kind)}, {"mount_ids", nlohmann::json::array()}};
            for(const auto *alias : mounts)
                payload["mount_ids"].push_back(alias->mount_id);
            auto status = CollectStatus::NotCollected;
            std::optional<ReadFailure> failure;
            if((kind == FilesystemKind::Local || kind == FilesystemKind::Memory) && mount.filesystem.value != "ramfs")
            {
                struct statvfs values
                {
                };
                std::string note;
                failure = read_statistics(mount, values, note);
                if(failure)
                {
                    status = CollectStatus::Failed;
                    if(!note.empty())
                        payload["note"] = note;
                }
                else
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
            else
                payload["note"] = kind == FilesystemKind::Network || kind == FilesystemKind::Userspace
                                      ? "Listed without querying remote or userspace storage"
                                      : "Capacity query not applicable or filesystem type unrecognized";
            add_record(raw, RawSource::FilesystemUsage, "statvfs/" + std::to_string(mount.mount_id), payload.dump(),
                       status, failure);
        }
    } // namespace

    void read_filesystems(RawStore &raw)
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
                    query(raw, aliases);
                }
                return;
            }
    }
} // namespace sysal::reader
