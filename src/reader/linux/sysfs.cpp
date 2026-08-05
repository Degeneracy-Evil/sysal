/// @file sysfs.cpp
/// @brief Linux sysfs 采集器实现
/// @details 从 /sys 采集原始数据写入 RawStore，遍历 CPU 拓扑、NUMA 节点、
///          网络接口、PCI 设备、块设备及 DMI 信息。

#include "reader/linux/sysfs.hpp"
#include "reader/linux/file_utils.hpp"

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
            auto content = read_file(path);
            if(content)
            {
                add_record(raw, source, path, *content, CollectStatus::Success);
            }
            else
            {
                add_record(raw, source, path, "", CollectStatus::Failed);
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

                // cpufreq 文件
                read_sysfs_file(raw, RawSource::SysfsCpu, (dir / "cpufreq" / "base_frequency").string());
                read_sysfs_file(raw, RawSource::SysfsCpu, (dir / "cpufreq" / "scaling_max_freq").string());
                read_sysfs_file(raw, RawSource::SysfsCpu, (dir / "cpufreq" / "scaling_governor").string());

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

        /// @brief 采集 PCI 设备信息
        /// @param raw 原始证据存储
        /// @details 遍历 /sys/bus/pci/devices，读取 vendor、device、class、numa_node
        void read_pci_sysfs(RawStore &raw)
        {
            const fs::path pci_base = "/sys/bus/pci/devices";
            if(!fs::exists(pci_base))
            {
                add_record(raw, RawSource::SysfsPci, pci_base.string(), "", CollectStatus::Failed);
                return;
            }

            bool found_any = false;
            std::error_code ec;
            for(const auto &entry : fs::directory_iterator(pci_base, ec))
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

                read_sysfs_file(raw, RawSource::SysfsPci, (dir / "vendor").string());
                read_sysfs_file(raw, RawSource::SysfsPci, (dir / "device").string());
                read_sysfs_file(raw, RawSource::SysfsPci, (dir / "class").string());
                read_sysfs_file(raw, RawSource::SysfsPci, (dir / "numa_node").string());
            }

            if(!found_any)
            {
                add_record(raw, RawSource::SysfsPci, pci_base.string(), "", CollectStatus::Failed);
            }
        }

        /// @brief 采集块设备信息
        /// @param raw 原始证据存储
        /// @details 遍历 /sys/block，读取 size 和 device/ 子目录
        void read_block_sysfs(RawStore &raw)
        {
            const fs::path block_base = "/sys/block";
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

                // device/ 子目录下的型号设备
                if(fs::exists(dir / "device"))
                {
                    read_sysfs_file(raw, RawSource::SysfsBlock, (dir / "device" / "model").string());
                }

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
                add_record(raw, RawSource::SysfsDmi, dmi_base.string(), "", CollectStatus::Failed);
                return;
            }

            read_sysfs_file(raw, RawSource::SysfsDmi, (dmi_base / "bios_vendor").string());
            read_sysfs_file(raw, RawSource::SysfsDmi, (dmi_base / "bios_version").string());
            read_sysfs_file(raw, RawSource::SysfsDmi, (dmi_base / "bios_date").string());
            read_sysfs_file(raw, RawSource::SysfsDmi, (dmi_base / "product_name").string());
            read_sysfs_file(raw, RawSource::SysfsDmi, (dmi_base / "product_serial").string());
            read_sysfs_file(raw, RawSource::SysfsDmi, (dmi_base / "sys_vendor").string());

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

        /// @brief 采集 EDAC 内存 DIMM 信息
        /// @param raw 原始证据存储
        /// @details 遍历 /sys/devices/system/edac/mc/mcN/dimmM/，读取各 DIMM 的
        ///          mem_type、size、label、location、dev_type、edac_mode。
        ///          无 EDAC 的系统（容器、消费级硬件）静默跳过。
        void read_edac_sysfs(RawStore &raw)
        {
            const fs::path edac_base = "/sys/devices/system/edac/mc";
            if(!fs::exists(edac_base))
            {
                return;
            }

            std::error_code ec;
            for(const auto &mc_entry : fs::directory_iterator(edac_base, ec))
            {
                if(!mc_entry.is_directory())
                {
                    continue;
                }
                auto mc_name = mc_entry.path().filename().string();
                if(mc_name.size() < 3 || mc_name.substr(0, 2) != "mc" ||
                   mc_name.find_first_not_of("0123456789", 2) != std::string::npos)
                {
                    continue;
                }

                for(const auto &dimm_entry : fs::directory_iterator(mc_entry.path(), ec))
                {
                    if(!dimm_entry.is_directory())
                    {
                        continue;
                    }
                    auto dimm_name = dimm_entry.path().filename().string();
                    if(dimm_name.size() < 5 || dimm_name.substr(0, 4) != "dimm" ||
                       dimm_name.find_first_not_of("0123456789", 4) != std::string::npos)
                    {
                        continue;
                    }

                    const auto &dir = dimm_entry.path();
                    read_sysfs_file(raw, RawSource::SysfsEdac, (dir / "dimm_mem_type").string());
                    read_sysfs_file(raw, RawSource::SysfsEdac, (dir / "size").string());
                    read_sysfs_file(raw, RawSource::SysfsEdac, (dir / "dimm_label").string());
                    read_sysfs_file(raw, RawSource::SysfsEdac, (dir / "dimm_location").string());
                    read_sysfs_file(raw, RawSource::SysfsEdac, (dir / "dimm_dev_type").string());
                    read_sysfs_file(raw, RawSource::SysfsEdac, (dir / "dimm_edac_mode").string());
                }
            }
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
            {Collect::Memory, read_numa_sysfs},
            {Collect::Network, read_net_sysfs},
            {Collect::Pci, read_pci_sysfs},
            {Collect::Storage, read_block_sysfs},
            {Collect::Platform, read_dmi_sysfs},
            {Collect::Platform, read_hypervisor_type},
            {Collect::Memory, read_edac_sysfs},
            {Collect::Cpu, read_thermal_sysfs},
        };

        for(const auto &entry : reader_dispatch)
        {
            if(has(flags, entry.flag))
            {
                entry.read(raw);
            }
        }
    }

} // namespace sysal::reader
