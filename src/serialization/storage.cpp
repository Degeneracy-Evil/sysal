#include "serialization/storage.hpp"
#include "serialization/json_values.hpp"
#include "serialization/storage_connections.hpp"
#include "serialization/storage_health.hpp"

#include <limits>
#include <utility>

namespace sysal::detail
{
    using json = nlohmann::json;

    namespace
    {
        [[nodiscard]] json device_number_to_json(const DeviceNumber &number)
        {
            return json{{"major", number.major}, {"minor", number.minor}};
        }
        [[nodiscard]] DeviceNumber device_number_from_json(const json &j)
        {
            return DeviceNumber{uint32_from_json(j.at("major"), "major"), uint32_from_json(j.at("minor"), "minor")};
        }

        [[nodiscard]] json storage_mount_to_json(const StorageMount &m)
        {
            json j{{"mount_id", m.mount_id},
                   {"parent_mount_id", m.parent_mount_id},
                   {"device_number", device_number_to_json(m.device_number)},
                   {"root", m.root},
                   {"path", m.path.value},
                   {"filesystem", m.filesystem.value},
                   {"source", m.source},
                   {"options", m.options},
                   {"read_only", m.read_only}};
            if(m.block_device)
                j["block_device"] = m.block_device->value;
            return j;
        }
        [[nodiscard]] StorageMount storage_mount_from_json(const json &j)
        {
            StorageMount m;
            m.mount_id = uint32_from_json(j.at("mount_id"), "mount_id");
            m.parent_mount_id = uint32_from_json(j.at("parent_mount_id"), "parent_mount_id");
            m.device_number = device_number_from_json(j.at("device_number"));
            m.root = j.at("root").get<std::string>();
            m.path = MountPoint{j.at("path").get<std::string>()};
            m.filesystem = FilesystemType{j.at("filesystem").get<std::string>()};
            m.source = j.at("source").get<std::string>();
            m.options = str_array_from_json(j.at("options"));
            m.read_only = j.at("read_only").get<bool>();
            if(j.contains("block_device"))
                m.block_device = DeviceName{j.at("block_device").get<std::string>()};
            return m;
        }

        [[nodiscard]] json storage_dev_to_json(const StorageDevice &sd)
        {
            json j = {
                {"id", sd.id.value()},
                {"name", sd.name.value},
                {"kind", static_cast<std::uint32_t>(sd.kind)},
            };
            if(sd.capacity)
            {
                j["capacity"] = sd.capacity->value;
            }
            if(sd.pci_address)
            {
                j["pci_address"] = pci_address_to_json(*sd.pci_address);
            }
            if(sd.mount_point)
            {
                j["mount_point"] = sd.mount_point->value;
            }
            if(sd.fs_type)
            {
                j["fs_type"] = sd.fs_type->value;
            }
            if(!sd.model.empty())
            {
                j["model"] = sd.model;
            }
            if(!sd.vendor.value.empty())
            {
                j["vendor"] = sd.vendor.value;
            }
            if(!sd.serial.empty())
            {
                j["serial"] = sd.serial;
            }
            if(!sd.firmware_revision.empty())
            {
                j["firmware_revision"] = sd.firmware_revision;
            }
            if(!sd.wwid.empty())
            {
                j["wwid"] = sd.wwid;
            }
            if(!sd.scheduler.empty())
            {
                j["scheduler"] = sd.scheduler;
            }
            if(sd.logical_block_size)
            {
                j["logical_block_size"] = sd.logical_block_size->value;
            }
            if(sd.physical_block_size)
            {
                j["physical_block_size"] = sd.physical_block_size->value;
            }
            if(sd.minimum_io_size)
            {
                j["minimum_io_size"] = sd.minimum_io_size->value;
            }
            if(sd.optimal_io_size)
            {
                j["optimal_io_size"] = sd.optimal_io_size->value;
            }
            if(sd.rotational)
            {
                j["rotational"] = *sd.rotational;
            }
            if(sd.read_only)
            {
                j["read_only"] = *sd.read_only;
            }
            if(sd.removable)
            {
                j["removable"] = *sd.removable;
            }
            if(sd.numa_node)
            {
                j["numa_node"] = sd.numa_node->value();
            }
            if(!sd.transport.empty())
            {
                j["transport"] = sd.transport;
            }
            if(!sd.controller_name.empty())
            {
                j["controller_name"] = sd.controller_name;
            }
            if(!sd.layer.empty())
            {
                j["layer"] = sd.layer;
            }
            if(!sd.mapper_name.empty())
            {
                j["mapper_name"] = sd.mapper_name;
            }
            if(!sd.mapper_uuid.empty())
            {
                j["mapper_uuid"] = sd.mapper_uuid;
            }
            if(!sd.raid_level.empty())
            {
                j["raid_level"] = sd.raid_level;
            }
            if(!sd.raid_state.empty())
            {
                j["raid_state"] = sd.raid_state;
            }
            if(sd.partition_number)
            {
                j["partition_number"] = *sd.partition_number;
            }
            if(sd.parent)
            {
                j["parent"] = sd.parent->value;
            }
            if(sd.raid_disks)
            {
                j["raid_disks"] = *sd.raid_disks;
            }
            if(sd.raid_degraded)
            {
                j["raid_degraded"] = *sd.raid_degraded;
            }
            if(!sd.slaves.empty())
            {
                j["slaves"] = json::array();
                for(const auto &item : sd.slaves)
                    j["slaves"].push_back(item.value);
            }
            if(sd.device_number)
                j["device_number"] = device_number_to_json(*sd.device_number);
            detail::storage_device_connections_to_json(j, sd);
            return j;
        }

        [[nodiscard]] StorageDevice storage_dev_from_json(const json &j)
        {
            StorageDevice sd;
            sd.id = StorageId(uint32_from_json(j.at("id"), "id"));
            j.at("name").get_to(sd.name.value);
            if(j.contains("capacity"))
            {
                sd.capacity = MemorySize{uint64_from_json(j.at("capacity"), "capacity")};
            }
            if(j.contains("pci_address"))
            {
                sd.pci_address = pci_address_from_json(j.at("pci_address"));
            }
            if(j.contains("mount_point"))
            {
                sd.mount_point = MountPoint{j.at("mount_point").get<std::string>()};
            }
            if(j.contains("fs_type"))
            {
                sd.fs_type = FilesystemType{j.at("fs_type").get<std::string>()};
            }
            sd.kind = validate_enum(uint32_from_json(j.at("kind"), "kind"), StorageKind::Other, "kind");
            sd.model = j.value("model", std::string{});
            sd.vendor = Vendor{j.value("vendor", std::string{})};
            sd.serial = j.value("serial", std::string{});
            sd.firmware_revision = j.value("firmware_revision", std::string{});
            sd.wwid = j.value("wwid", std::string{});
            sd.scheduler = j.value("scheduler", std::string{});
            if(j.contains("logical_block_size"))
            {
                sd.logical_block_size = MemorySize{uint64_from_json(j.at("logical_block_size"), "logical_block_size")};
            }
            if(j.contains("physical_block_size"))
            {
                sd.physical_block_size =
                    MemorySize{uint64_from_json(j.at("physical_block_size"), "physical_block_size")};
            }
            if(j.contains("minimum_io_size"))
            {
                sd.minimum_io_size = MemorySize{uint64_from_json(j.at("minimum_io_size"), "minimum_io_size")};
            }
            if(j.contains("optimal_io_size"))
            {
                sd.optimal_io_size = MemorySize{uint64_from_json(j.at("optimal_io_size"), "optimal_io_size")};
            }
            if(j.contains("rotational"))
            {
                sd.rotational = j.at("rotational").get<bool>();
            }
            if(j.contains("read_only"))
            {
                sd.read_only = j.at("read_only").get<bool>();
            }
            if(j.contains("removable"))
            {
                sd.removable = j.at("removable").get<bool>();
            }
            if(j.contains("numa_node"))
            {
                sd.numa_node = NumaNodeId{uint32_from_json(j.at("numa_node"), "numa_node")};
            }
            sd.transport = j.value("transport", std::string{});
            sd.controller_name = j.value("controller_name", std::string{});
            sd.layer = j.value("layer", std::string{});
            sd.mapper_name = j.value("mapper_name", std::string{});
            sd.mapper_uuid = j.value("mapper_uuid", std::string{});
            sd.raid_level = j.value("raid_level", std::string{});
            sd.raid_state = j.value("raid_state", std::string{});
            if(j.contains("partition_number"))
            {
                sd.partition_number = uint32_from_json(j.at("partition_number"), "partition_number");
            }
            if(j.contains("parent"))
            {
                sd.parent = DeviceName{j.at("parent").get<std::string>()};
            }
            if(j.contains("raid_disks"))
            {
                sd.raid_disks = uint32_from_json(j.at("raid_disks"), "raid_disks");
            }
            if(j.contains("raid_degraded"))
            {
                sd.raid_degraded = uint32_from_json(j.at("raid_degraded"), "raid_degraded");
            }
            if(j.contains("slaves"))
                for(const auto &item : j.at("slaves"))
                    sd.slaves.push_back(DeviceName{item.get<std::string>()});
            if(j.contains("device_number"))
                sd.device_number = device_number_from_json(j.at("device_number"));
            detail::storage_device_connections_from_json(j, sd);
            return sd;
        }

    } // namespace

    [[nodiscard]] json storage_to_json(const Storage &s)
    {
        json arr = json::array();
        for(const auto &dev : s.devices)
        {
            arr.push_back(storage_dev_to_json(dev));
        }
        json j{{"devices", std::move(arr)}};
        if(!s.health.empty())
        {
            j["health"] = json::array();
            for(const auto &report : s.health)
                j["health"].push_back(detail::storage_health_to_json(report));
        }
        if(!s.mounts.empty())
        {
            j["mounts"] = json::array();
            for(const auto &mount : s.mounts)
                j["mounts"].push_back(storage_mount_to_json(mount));
        }
        detail::storage_connections_to_json(j, s);
        return j;
    }

    [[nodiscard]] Storage storage_from_json(const json &j)
    {
        Storage s;
        for(const auto &elem : j.at("devices"))
        {
            s.devices.push_back(storage_dev_from_json(elem));
        }
        if(j.contains("health"))
            for(const auto &report : j.at("health"))
                s.health.push_back(detail::storage_health_from_json(report));
        if(j.contains("mounts"))
            for(const auto &mount : j.at("mounts"))
                s.mounts.push_back(storage_mount_from_json(mount));
        detail::storage_connections_from_json(j, s);
        return s;
    }

} // namespace sysal::detail
