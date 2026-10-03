/// @file sysfs.cpp
/// @brief Linux sysfs 采集器实现
/// @details 从 /sys 采集原始数据写入 RawStore，遍历 CPU 拓扑、NUMA 节点、
///          网络接口、PCI 设备、块设备及 DMI 信息。

#include "reader/linux/sysfs.hpp"
#include "reader/linux/file_utils.hpp"
#include "reader/linux/memory_topology.hpp"
#include "reader/linux/network_capabilities.hpp"
#include "reader/linux/pci.hpp"
#include "reader/linux/rdma.hpp"
#include "reader/linux/sensors.hpp"
#include "reader/linux/storage_connections.hpp"
#include "reader/linux/storage_health.hpp"

#include <filesystem>
#include <string>

namespace sysal::reader
{

    namespace
    {

        namespace fs = std::filesystem;

        /// @brief 读取 sysfs 文件并添加记录，失败时记录 Failed 状态
        /// @param raw 原始证据存储
        /// @param source 原始数据来源
        /// @param path 文件路径
        void read_sysfs_file(RawStore &raw, RawSource source, const std::string &path)
        {
            read_file_record(raw, source, path);
        }

        void read_frequency_files(RawStore &raw, const fs::path &directory)
        {
            for(const auto *field :
                {"related_cpus", "affected_cpus", "base_frequency", "cpuinfo_min_freq", "cpuinfo_max_freq",
                 "scaling_min_freq", "scaling_max_freq", "scaling_cur_freq", "cpuinfo_cur_freq", "scaling_driver",
                 "scaling_governor", "energy_performance_preference"})
            {
                read_sysfs_file(raw, RawSource::SysfsCpu, (directory / field).string());
            }
        }

        /// @brief 采集 CPU 拓扑信息
        /// @param raw 原始证据存储
        /// @details 遍历 /sys/devices/system/cpu/cpuN，读取 topology、online、cpufreq 文件
        void read_cpu_sysfs(RawStore &raw)
        {
            const fs::path cpu_base = "/sys/devices/system/cpu";
            if(!fs::exists(cpu_base))
            {
                add_record(raw, RawSource::SysfsCpu, cpu_base.string(), "", CollectStatus::Failed);
                return;
            }

            for(const auto *field : {"present", "online", "smt/active", "smt/control", "cpufreq/boost"})
            {
                read_sysfs_file(raw, RawSource::SysfsCpu, (cpu_base / field).string());
            }
            const auto policy_base = cpu_base / "cpufreq";
            bool policies_collected = false;
            std::error_code policy_ec;
            if(fs::is_directory(policy_base, policy_ec))
            {
                for(const auto &policy : fs::directory_iterator(policy_base, policy_ec))
                {
                    const auto name = policy.path().filename().string();
                    if(name.size() <= 6 || !name.starts_with("policy") ||
                       name.find_first_not_of("0123456789", 6) != std::string::npos)
                    {
                        continue;
                    }
                    policies_collected = true;
                    read_frequency_files(raw, policy.path());
                }
            }

            bool found_any = false;
            std::error_code ec;
            for(const auto &entry : fs::directory_iterator(cpu_base, ec))
            {
                if(!entry.is_directory())
                {
                    continue;
                }
                auto name = entry.path().filename().string();
                // 仅处理 cpuN 目录
                if(name.size() < 4 || name.substr(0, 3) != "cpu" ||
                   name.find_first_not_of("0123456789", 3) != std::string::npos)
                {
                    continue;
                }

                found_any = true;
                const auto &dir = entry.path();

                // topology 文件
                read_sysfs_file(raw, RawSource::SysfsCpu, (dir / "topology" / "physical_package_id").string());
                read_sysfs_file(raw, RawSource::SysfsCpu, (dir / "topology" / "core_id").string());

                // online 状态
                read_sysfs_file(raw, RawSource::SysfsCpu, (dir / "online").string());

                // 旧内核可能仅提供每个 CPU 的 cpufreq 目录。
                if(!policies_collected)
                {
                    std::error_code frequency_ec;
                    if(fs::is_directory(dir / "cpufreq", frequency_ec))
                        read_frequency_files(raw, dir / "cpufreq");
                }
                else
                {
                    read_sysfs_file(raw, RawSource::SysfsCpu, (dir / "cpufreq" / "base_frequency").string());
                    read_sysfs_file(raw, RawSource::SysfsCpu, (dir / "cpufreq" / "cpuinfo_max_freq").string());
                }

                // 缓存目录：cpuN/cache/indexM/{level,type,size,ways_of_associativity,line_size}
                std::error_code cache_ec;
                const auto cache_dir = dir / "cache";
                if(fs::is_directory(cache_dir, cache_ec))
                {
                    for(const auto &cache_entry : fs::directory_iterator(cache_dir, cache_ec))
                    {
                        const auto &cache_name = cache_entry.path().filename().string();
                        if(cache_name.size() < 6 || cache_name.substr(0, 5) != "index")
                        {
                            continue;
                        }
                        for(const auto *field : {"id", "number_of_sets", "shared_cpu_list"})
                        {
                            read_sysfs_file(raw, RawSource::SysfsCpu, (cache_entry.path() / field).string());
                        }
                        read_sysfs_file(raw, RawSource::SysfsCpu, (cache_entry.path() / "level").string());
                        read_sysfs_file(raw, RawSource::SysfsCpu, (cache_entry.path() / "type").string());
                        read_sysfs_file(raw, RawSource::SysfsCpu, (cache_entry.path() / "size").string());
                        read_sysfs_file(raw, RawSource::SysfsCpu,
                                        (cache_entry.path() / "ways_of_associativity").string());
                        read_sysfs_file(raw, RawSource::SysfsCpu,
                                        (cache_entry.path() / "coherency_line_size").string());
                    }
                }
            }

            if(!found_any)
            {
                add_record(raw, RawSource::SysfsCpu, cpu_base.string(), "", CollectStatus::Failed);
            }
        }

        /// @brief 采集 NUMA 节点信息
        /// @param raw 原始证据存储
        /// @details 遍历 /sys/devices/system/node/nodeN，读取 cpulist 和 meminfo
        void read_numa_sysfs(RawStore &raw)
        {
            const fs::path node_base = "/sys/devices/system/node";
            if(!fs::exists(node_base))
            {
                add_record(raw, RawSource::SysfsNuma, node_base.string(), "", CollectStatus::Failed);
                return;
            }

            bool found_any = false;
            std::error_code ec;
            for(const auto &entry : fs::directory_iterator(node_base, ec))
            {
                if(!entry.is_directory())
                {
                    continue;
                }
                auto name = entry.path().filename().string();
                if(name.size() < 5 || name.substr(0, 4) != "node" ||
                   name.find_first_not_of("0123456789", 4) != std::string::npos)
                {
                    continue;
                }

                found_any = true;
                const auto &dir = entry.path();

                read_sysfs_file(raw, RawSource::SysfsNuma, (dir / "cpulist").string());
                read_sysfs_file(raw, RawSource::SysfsNuma, (dir / "meminfo").string());
            }

            if(!found_any)
            {
                add_record(raw, RawSource::SysfsNuma, node_base.string(), "", CollectStatus::Failed);
            }
        }

        /// @brief 采集网络接口信息
        /// @param raw 原始证据存储
        /// @details 遍历 /sys/class/net，读取 address、operstate、speed
        void read_net_sysfs(RawStore &raw)
        {
            const fs::path net_base = "/sys/class/net";
            if(!fs::exists(net_base))
            {
                add_record(raw, RawSource::SysfsNet, net_base.string(), "", CollectStatus::Failed);
                return;
            }

            bool found_any = false;
            std::error_code ec;
            for(const auto &entry : fs::directory_iterator(net_base, ec))
            {
                if(!entry.is_directory() && !entry.is_symlink())
                {
                    continue;
                }
                auto name = entry.path().filename().string();
                if(name.empty())
                {
                    continue;
                }

                found_any = true;
                const auto &dir = entry.path();

                read_sysfs_file(raw, RawSource::SysfsNet, (dir / "address").string());
                read_sysfs_file(raw, RawSource::SysfsNet, (dir / "operstate").string());
                read_sysfs_file(raw, RawSource::SysfsNet, (dir / "speed").string());

                static constexpr std::string_view fields[] = {"mtu",     "carrier",        "duplex",
                                                              "ifindex", "phys_port_name", "device/numa_node"};
                for(auto field : fields)
                {
                    read_sysfs_file(raw, RawSource::SysfsNet, (dir / field).string());
                }
                std::error_code driver_ec;
                auto driver = fs::read_symlink(dir / "device" / "driver", driver_ec);
                if(!driver_ec)
                {
                    add_record(raw, RawSource::SysfsNet, (dir / "driver").string(), driver.filename().string(),
                               CollectStatus::Success);
                }

                std::error_code master_ec;
                const auto master = fs::read_symlink(dir / "master", master_ec);
                if(!master_ec)
                    add_record(raw, RawSource::SysfsNet, (dir / "master").string(), master.filename().string(),
                               CollectStatus::Success);
                std::error_code lowers_ec;
                std::string lowers;
                for(const auto &link : fs::directory_iterator(dir, lowers_ec))
                {
                    const auto filename = link.path().filename().string();
                    if(filename.starts_with("lower_"))
                        lowers += filename.substr(6) + "\n";
                }
                add_record(raw, RawSource::SysfsNet, (dir / "lower_interfaces").string(), lowers,
                           lowers_ec ? CollectStatus::Failed : CollectStatus::Success,
                           lowers_ec ? std::optional{file_failure(lowers_ec.value())} : std::nullopt);
                const bool physical = fs::exists(dir / "device");
                const auto kind = fs::exists(dir / "bridge")    ? "bridge"
                                  : fs::exists(dir / "bonding") ? "bond"
                                  : physical                    ? "physical"
                                                                : "virtual";
                add_record(raw, RawSource::SysfsNet, (dir / "kind").string(), kind, CollectStatus::Success);
                if(std::string_view(kind) == "bond")
                    read_sysfs_file(raw, RawSource::SysfsNet, (dir / "bonding" / "mode").string());
                if(physical)
                    read_network_capabilities(raw, name);

                // device 符号链接 → PCI 地址（虚拟接口如 lo 无此链接，静默跳过）
                std::error_code link_ec;
                auto device_target = fs::read_symlink(dir / "device", link_ec);
                if(!link_ec)
                {
                    add_record(raw, RawSource::SysfsNet, (dir / "device").string(), device_target.string(),
                               CollectStatus::Success);
                }
            }

            if(!found_any)
            {
                add_record(raw, RawSource::SysfsNet, net_base.string(), "", CollectStatus::Failed);
            }
        }

        /// @brief 采集块设备信息
        /// @param raw 原始证据存储
        /// @details 遍历 /sys/block，读取 size 和 device/ 子目录
        void read_block_sysfs(RawStore &raw)
        {
            const fs::path block_base = "/sys/class/block";
            if(!fs::exists(block_base))
            {
                add_record(raw, RawSource::SysfsBlock, block_base.string(), "", CollectStatus::Failed);
                return;
            }

            bool found_any = false;
            std::error_code ec;
            for(const auto &entry : fs::directory_iterator(block_base, ec))
            {
                if(!entry.is_directory() && !entry.is_symlink())
                {
                    continue;
                }
                auto name = entry.path().filename().string();
                if(name.empty())
                {
                    continue;
                }

                found_any = true;
                const auto &dir = entry.path();

                read_sysfs_file(raw, RawSource::SysfsBlock, (dir / "size").string());

                // queue/rotational: "0"=SSD, "1"=HDD
                read_sysfs_file(raw, RawSource::SysfsBlock, (dir / "queue" / "rotational").string());

                static constexpr std::string_view fields[] = {"ro",
                                                              "removable",
                                                              "wwid",
                                                              "device/model",
                                                              "device/vendor",
                                                              "device/serial",
                                                              "device/rev",
                                                              "device/firmware_rev",
                                                              "device/numa_node",
                                                              "device/wwid",
                                                              "device/transport",
                                                              "queue/logical_block_size",
                                                              "queue/physical_block_size",
                                                              "queue/minimum_io_size",
                                                              "queue/optimal_io_size",
                                                              "queue/scheduler"};
                for(auto field : fields)
                {
                    read_sysfs_file(raw, RawSource::SysfsBlock, (dir / field).string());
                }

                for(const auto *field : {"dev", "partition", "dm/name", "dm/uuid", "md/level", "md/array_state",
                                         "md/raid_disks", "md/degraded"})
                    read_sysfs_file(raw, RawSource::SysfsBlock, (dir / field).string());
                if(fs::exists(dir / "partition"))
                {
                    std::error_code parent_ec;
                    const auto target = fs::canonical(dir, parent_ec);
                    if(!parent_ec)
                        add_record(raw, RawSource::SysfsBlock, (dir / "parent").string(),
                                   target.parent_path().filename().string(), CollectStatus::Success);
                }
                std::error_code slaves_ec;
                std::string slaves;
                for(const auto &slave : fs::directory_iterator(dir / "slaves", slaves_ec))
                    slaves += slave.path().filename().string() + "\n";
                add_record(raw, RawSource::SysfsBlock, (dir / "slaves").string(), slaves,
                           slaves_ec ? CollectStatus::Failed : CollectStatus::Success,
                           slaves_ec ? std::optional{file_failure(slaves_ec.value())} : std::nullopt);

                // 块设备入口本身是符号链接，目标指向 PCI 设备树，如
                //   /sys/block/nvme0n1 -> ../devices/pci0000:e2/0000:e2:04.0/0000:e4:00.0/nvme/nvme0/nvme0n1
                // 该目标的最后一个 PCI 地址段（如 0000:e4:00.0）即设备所属的 PCI 控制器。
                // 虚拟设备（loop/ram）目标为 ../devices/virtual/...，无 PCI 段，静默跳过。
                std::error_code link_ec;
                auto entry_target = fs::read_symlink(dir, link_ec);
                if(!link_ec)
                {
                    // 路径含分设备名，便于解析器提取设备名与文件名 "device"
                    add_record(raw, RawSource::SysfsBlock, (dir / "device").string(), entry_target.string(),
                               CollectStatus::Success);
                }
            }

            if(!found_any)
            {
                add_record(raw, RawSource::SysfsBlock, block_base.string(), "", CollectStatus::Failed);
            }
        }

        /// @brief 采集 DMI/BIOS 信息
        /// @param raw 原始证据存储
        /// @details 读取 /sys/class/dmi/id 下的固件与产品信息文件
        void read_dmi_sysfs(RawStore &raw)
        {
            const fs::path dmi_base = "/sys/class/dmi/id";
            if(!fs::exists(dmi_base))
            {
                add_record(raw, RawSource::SysfsDmi, dmi_base.string(), "", CollectStatus::NotCollected,
                           ReadFailure::NotPresent);
                return;
            }

            static constexpr std::string_view fields[] = {
                "bios_vendor",    "bios_version",      "bios_date",      "bios_release",   "ec_firmware_release",
                "sys_vendor",     "product_name",      "product_serial", "product_family", "product_version",
                "product_sku",    "product_uuid",      "board_vendor",   "board_name",     "board_version",
                "board_serial",   "board_asset_tag",   "chassis_vendor", "chassis_type",   "chassis_version",
                "chassis_serial", "chassis_asset_tag",
            };
            for(auto field : fields)
            {
                read_sysfs_file(raw, RawSource::SysfsDmi, (dmi_base / field).string());
            }

            // UEFI 检测：/sys/firmware/efi 在 UEFI 系统上存在
            if(fs::exists("/sys/firmware/efi"))
            {
                add_record(raw, RawSource::SysfsDmi, "/sys/firmware/efi", "1", CollectStatus::Success);
            }
        }

        /// @brief 采集 /sys/hypervisor/type
        /// @param raw 原始证据存储
        /// @details 读取 /sys/hypervisor/type 文件（Xen 等半虚拟化场景存在），
        ///          文件不存在时记录 Failed 状态。
        void read_hypervisor_type(RawStore &raw)
        {
            read_sysfs_file(raw, RawSource::SysHypervisor, "/sys/hypervisor/type");
        }

        /// @brief 采集温度传感器信息
        /// @param raw 原始证据存储
        /// @details 遍历 /sys/class/thermal/thermal_zoneN/，读取 type 与 temp
        ///          （temp 单位为毫摄氏度）。无热区（容器/虚拟化）时静默跳过。
        void read_thermal_sysfs(RawStore &raw)
        {
            const fs::path thermal_base = "/sys/class/thermal";
            if(!fs::exists(thermal_base))
            {
                return;
            }

            std::error_code ec;
            for(const auto &entry : fs::directory_iterator(thermal_base, ec))
            {
                if(!entry.is_directory())
                {
                    continue;
                }
                const auto &name = entry.path().filename().string();
                if(name.size() < 12 || name.substr(0, 12) != "thermal_zone")
                {
                    continue;
                }
                read_sysfs_file(raw, RawSource::SysfsThermal, (entry.path() / "type").string());
                read_sysfs_file(raw, RawSource::SysfsThermal, (entry.path() / "temp").string());
            }
        }

    } // namespace

    void read_sysfs(RawStore &raw, Collect flags)
    {
        struct ReaderDispatch
        {
            Collect flag;
            void (*read)(RawStore &);
        };

        static const ReaderDispatch reader_dispatch[] = {
            {Collect::Cpu, read_cpu_sysfs},
            {Collect::Cpu | Collect::Memory, read_numa_sysfs},
            {Collect::Network, read_net_sysfs},
            {Collect::Network, read_rdma_sysfs},
            {Collect::Pci | Collect::Network | Collect::Storage | Collect::StorageHealth, read_pci_sysfs},
            {Collect::Storage | Collect::StorageHealth, read_block_sysfs},
            {Collect::Storage | Collect::StorageHealth, read_storage_connections},
            {Collect::Platform, read_dmi_sysfs},
            {Collect::Platform, read_hypervisor_type},
            {Collect::Memory, read_edac_sysfs},
            {Collect::Sensors, read_sensors},
            {Collect::StorageHealth, read_storage_health},
        };

        for(const auto &entry : reader_dispatch)
        {
            if(has(flags, entry.flag))
            {
                entry.read(raw);
            }
        }
        if(has(flags, Collect::Cpu) && !has(flags, Collect::Sensors))
            read_thermal_sysfs(raw);
    }

} // namespace sysal::reader
