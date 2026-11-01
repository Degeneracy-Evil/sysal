#include "parser/storage_compatibility.hpp"

namespace sysal::detail
{
    void apply_storage_compatibility(StorageDevice &device, std::string_view sysfs_target)
    {
        device.layer = device.partition_number                                      ? "partition"
                       : !device.mapper_uuid.empty() || !device.mapper_name.empty() ? "device-mapper"
                       : !device.raid_level.empty()                                 ? "md"
                       : sysfs_target.find("/virtual/") != std::string_view::npos   ? "virtual"
                       : sysfs_target.empty()                                       ? "unknown"
                                                                                    : "disk";
        // Historical name heuristics are confined to deprecated compatibility output.
        device.kind = StorageKind::Other;
        if(device.name.value.starts_with("nvme"))
        {
            device.kind = StorageKind::Nvme;
            return;
        }
        for(const auto prefix : {"loop", "ram", "sr", "dm-", "md", "zram", "fd"})
            if(device.name.value.starts_with(prefix))
                return;
        if(device.rotational)
            device.kind = *device.rotational ? StorageKind::Hdd : StorageKind::Ssd;
    }
} // namespace sysal::detail
