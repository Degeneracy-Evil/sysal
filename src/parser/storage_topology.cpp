#include "parser/storage_topology.hpp"
#include "parser/parse_utils.hpp"

#include <limits>
#include <map>
#include <sstream>
#include <string_view>
#include <utility>

namespace sysal::detail
{
    namespace
    {
        std::string unescape_mount(std::string_view value)
        {
            std::string decoded;
            for(std::size_t i = 0; i < value.size(); ++i)
            {
                if(value[i] == '\\' && i + 3 < value.size() && value[i + 1] >= '0' && value[i + 1] <= '7' &&
                   value[i + 2] >= '0' && value[i + 2] <= '7' && value[i + 3] >= '0' && value[i + 3] <= '7')
                {
                    const int octal = (value[i + 1] - '0') * 64 + (value[i + 2] - '0') * 8 + value[i + 3] - '0';
                    decoded += static_cast<char>(octal);
                    i += 3;
                }
                else
                    decoded += value[i];
            }
            return decoded;
        }

        void parse_mountinfo(Storage &storage, std::string_view payload, std::vector<std::string> &warnings)
        {
            for(const auto &line : split(payload, '\n'))
            {
                if(line.empty())
                    continue;
                std::istringstream stream(line);
                std::string id, parent, number, root, path, options, token;
                if(!(stream >> id >> parent >> number >> root >> path >> options))
                {
                    warnings.push_back("parse_storage: malformed mountinfo header");
                    continue;
                }
                while(stream >> token && token != "-")
                {
                }
                std::string filesystem, source, super_options;
                auto mount_id = parse_uint(id);
                auto parent_id = parse_uint(parent);
                auto device_number = parse_device_number(number);
                if(token != "-" || !(stream >> filesystem >> source >> super_options) || !mount_id || !parent_id ||
                   *mount_id > std::numeric_limits<std::uint32_t>::max() ||
                   *parent_id > std::numeric_limits<std::uint32_t>::max() || !device_number)
                {
                    warnings.push_back("parse_storage: malformed mountinfo record");
                    continue;
                }
                StorageMount mount;
                mount.mount_id = static_cast<std::uint32_t>(*mount_id);
                mount.parent_mount_id = static_cast<std::uint32_t>(*parent_id);
                mount.device_number = *device_number;
                mount.root = unescape_mount(root);
                mount.path = MountPoint{unescape_mount(path)};
                mount.filesystem = FilesystemType{filesystem};
                mount.source = unescape_mount(source);
                mount.options = split(options, ',');
                for(const auto &option : mount.options)
                    mount.read_only |= option == "ro";
                storage.mounts.push_back(std::move(mount));
            }
        }
    } // namespace

    void apply_storage_mounts(Storage &storage, const RawStore &raw, std::vector<std::string> &warnings)
    {
        bool mountinfo_read = false;
        for(const auto *record : raw.get_all(RawSource::StorageMountInfo))
            if(record->status == CollectStatus::Success)
            {
                parse_mountinfo(storage, record->payload, warnings);
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
