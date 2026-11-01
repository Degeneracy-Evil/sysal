#include "common/filesystem.hpp"
namespace sysal::detail
{
    FilesystemKind filesystem_kind(std::string_view type)
    {
        if(type == "tmpfs" || type == "devtmpfs" || type == "ramfs")
            return FilesystemKind::Memory;
        for(auto local : {"ext2", "ext3", "ext4", "xfs", "btrfs", "f2fs", "jfs", "reiserfs", "zfs", "bcachefs", "vfat",
                          "exfat", "ntfs", "ntfs3", "iso9660", "udf", "squashfs", "overlay"})
            if(type == local)
                return FilesystemKind::Local;
        for(auto network : {"nfs", "nfs4", "cifs", "smb3", "ceph", "lustre", "beegfs", "gpfs", "afs", "9p",
                            "fuse.sshfs", "fuse.glusterfs", "fuse.ceph"})
            if(type == network)
                return FilesystemKind::Network;
        if(type == "fuse" || type == "fuseblk" || type.starts_with("fuse."))
            return FilesystemKind::Userspace;
        for(auto pseudo :
            {"proc", "sysfs", "cgroup", "cgroup2", "devpts", "securityfs", "debugfs", "tracefs", "pstore", "efivarfs",
             "configfs", "mqueue", "hugetlbfs", "binfmt_misc", "autofs", "rpc_pipefs", "fusectl", "bpf", "nsfs"})
            if(type == pseudo)
                return FilesystemKind::Pseudo;
        return FilesystemKind::Unknown;
    }

} // namespace sysal::detail
