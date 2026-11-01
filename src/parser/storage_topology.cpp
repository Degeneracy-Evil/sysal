#include "parser/storage_topology.hpp"
#include "common/mountinfo.hpp"

#include <map>

namespace sysal::detail
{
    void apply_storage_mounts(Storage &storage, const RawStore &raw, std::vector<std::string> &warnings)
    {
        bool mountinfo_read = false;
        for(const auto *record : raw.get_all(RawSource::StorageMountInfo))
            if(record->status == CollectStatus::Success)
            {
                storage.mounts = parse_mountinfo(record->payload, warnings);
                mountinfo_read = true;
                break;
            }
        if(!mountinfo_read)
            return; // Older replay evidence keeps its df-derived representative mount.
        std::map<std::pair<std::uint32_t, std::uint32_t>, std::size_t> numbers;
        for(std::size_t i = 0; i < storage.devices.size(); ++i)
        {
            auto &device = storage.devices[i];
            device.mount_point.reset();
            device.fs_type.reset();
            if(device.device_number)
                numbers[{device.device_number->major, device.device_number->minor}] = i;
        }
        for(auto &mount : storage.mounts)
        {
            const auto device = numbers.find({mount.device_number.major, mount.device_number.minor});
            if(device == numbers.end())
                continue; // Network/pseudo filesystems or a device outside the visible sysfs view.
            auto &block = storage.devices[device->second];
            mount.block_device = block.name;
            if(!block.mount_point || mount.path.value == "/")
            {
                block.mount_point = mount.path;
                block.fs_type = mount.filesystem;
            }
        }
    }
} // namespace sysal::detail
