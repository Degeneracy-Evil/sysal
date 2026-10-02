/// @file accelerator.cpp
/// @brief 加速器解析器实现
/// @details 从 nvidia-smi 命令输出和 sysfs PCI 数据中解析 GPU 等加速器信息。

#include "accelerator.hpp"

#include "parse_utils.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <nlohmann/json.hpp>
#include <string_view>
#include <unordered_map>

namespace sysal::detail
{

    namespace
    {

        std::string string_field(const nlohmann::json &value, const char *key, const std::string &fallback = "")
        {
            const auto field = value.find(key);
            return field != value.end() && field->is_string() ? field->get<std::string>() : fallback;
        }

        std::string uuid_hex(std::string value)
        {
            if(value.starts_with("GPU-") || value.starts_with("MIG-"))
                value.erase(0, 4);
            if(value.find('/') != std::string::npos)
                return "";
            value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
            for(auto &character : value)
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            return value.size() == 32 ? value : "";
        }

        /// @brief 解析 nvidia-smi CSV 输出中的单行
        /// @param line CSV 行内容（格式: index, name, memory.total, pci.bus_id, driver_version）
        /// @param warnings 警告列表
        /// @return 解析成功返回 AcceleratorDevice，否则返回 nullopt
        std::optional<AcceleratorDevice> parse_nvidia_smi_row(std::string_view line, std::vector<std::string> &warnings)
        {
            auto fields = split(line, ',');
            if(fields.size() < 4)
            {
                warnings.push_back("parse_accelerator: nvidia-smi CSV 字段不足: " + std::string(line));
                return std::nullopt;
            }

            AcceleratorDevice dev;
            dev.kind = AcceleratorKind::Gpu;
            dev.vendor = Vendor{"NVIDIA"};
            dev.visible_to_current_process = true;

            // 字段 0: index
            auto index_val = parse_uint(trim(fields[0]));
            if(!index_val.has_value())
            {
                warnings.push_back("parse_accelerator: nvidia-smi index 解析失败: " + trim(fields[0]));
                return std::nullopt;
            }
            dev.id = AcceleratorId{static_cast<std::uint32_t>(*index_val)};

            // 字段 1: name
            dev.name = DeviceName{trim(fields[1])};

            // 字段 2: memory.total（格式: "97536 MiB"）
            {
                auto mem_str = trim(fields[2]);
                // 提取数字部分和单位部分
                auto parts = split(mem_str, ' ');
                if(!parts.empty())
                {
                    auto num = parse_uint(trim(parts[0]));
                    if(num.has_value())
                    {
                        // 检查单位：MiB 或 GiB
                        std::uint64_t multiplier = 1;
                        if(parts.size() > 1)
                        {
                            auto unit = trim(parts[1]);
                            static const std::unordered_map<std::string, std::uint64_t> unit_multipliers = {
                                {"MiB", 1024ULL * 1024ULL},
                                {"GiB", 1024ULL * 1024ULL * 1024ULL},
                                {"KiB", 1024ULL},
                                {"B", 1ULL},
                            };

                            auto it = unit_multipliers.find(unit);
                            if(it != unit_multipliers.end())
                            {
                                multiplier = it->second;
                            }
                            else
                            {
                                // 未知单位，默认按 MiB 处理
                                warnings.push_back("parse_accelerator: nvidia-smi memory.total 未知单位: " + unit +
                                                   "，默认按 MiB 处理");
                                multiplier = 1024ULL * 1024ULL;
                            }
                        }
                        else
                        {
                            // 无单位，默认按 MiB 处理
                            multiplier = 1024ULL * 1024ULL;
                        }
                        dev.memory_size = MemorySize{*num * multiplier};
                    }
                    else
                    {
                        warnings.push_back("parse_accelerator: nvidia-smi memory.total 数值解析失败: " +
                                           trim(parts[0]));
                    }
                }
            }

            if(fields.size() > 5 && trim(fields[5]).starts_with("GPU-"))
                dev.uuid = trim(fields[5]);

            // 字段 3: pci.bus_id
            auto pci = parse_pci_address(trim(fields[3]));
            if(pci.has_value())
            {
                dev.pci_address = *pci;
            }
            else
            {
                warnings.push_back("parse_accelerator: nvidia-smi pci.bus_id 解析失败: " + trim(fields[3]));
            }

            return dev;
        }

        /// @brief 从 SysfsPci 记录中查找指定 PCI 地址的 NUMA 节点
        /// @param raw 原始证据存储
        /// @param pci_addr PCI 地址
        /// @return NUMA 节点 ID，未找到则返回 nullopt
        std::optional<NumaNodeId> find_numa_node_for_pci(const RawStore &raw, const PciAddress &pci_addr)
        {
            // 构建 PCI 地址字符串用于匹配 sysfs 路径
            // sysfs 路径格式: /sys/bus/pci/devices/DDDD:BB:DD.F/numa_node
            // PCI 地址格式: DDDD:BB:DD.F（十六进制）
            char addr_buf[32];
            std::snprintf(addr_buf, sizeof(addr_buf), "%04x:%02x:%02x.%x", static_cast<unsigned>(pci_addr.domain),
                          static_cast<unsigned>(pci_addr.bus), static_cast<unsigned>(pci_addr.device),
                          static_cast<unsigned>(pci_addr.function));

            auto pci_records = raw.get_all(RawSource::SysfsPci);
            for(const auto *rec : pci_records)
            {
                if(rec->status != CollectStatus::Success)
                {
                    continue;
                }

                const auto &path = rec->path_or_command;
                // 查找包含该 PCI 地址且以 numa_node 结尾的路径
                if(path.find(addr_buf) != std::string::npos && path.find("numa_node") != std::string::npos)
                {
                    auto node_val = parse_uint(trim(rec->payload));
                    if(node_val.has_value())
                    {
                        return NumaNodeId{static_cast<std::uint32_t>(*node_val)};
                    }
                }
            }

            return std::nullopt;
        }

        /// @brief 判断 nvidia-smi CSV 行是否为表头
        /// @param line CSV 行内容
        /// @return 是表头则返回 true
        bool is_nvidia_smi_header(std::string_view line)
        {
            auto trimmed = trim(line);
            return trimmed.find("index") != std::string_view::npos || trimmed.find("name") != std::string_view::npos;
        }

    } // namespace

    std::optional<Accelerators> parse_accelerator(const RawStore &raw, std::vector<std::string> &warnings)
    {
        Accelerators accelerators;
        const auto next_id = [&]()
        {
            std::uint32_t next = 0;
            for(const auto &device : accelerators.devices)
                next = std::max(next, device.id.value() + 1);
            return next;
        };
        const auto merge = [&](AcceleratorDevice device)
        {
            for(auto &existing : accelerators.devices)
            {
                const bool same_uuid = device.uuid && existing.uuid && device.uuid == existing.uuid;
                const bool same_physical =
                    !device.parent_uuid && !existing.parent_uuid &&
                    ((device.pci_address && existing.pci_address && device.pci_address == existing.pci_address) ||
                     (device.vendor.value == existing.vendor.value && device.id == existing.id));
                if(same_uuid || same_physical)
                {
                    if(!existing.uuid)
                        existing.uuid = device.uuid;
                    if(!existing.memory_size)
                        existing.memory_size = device.memory_size;
                    if(!existing.nearest_numa_node)
                        existing.nearest_numa_node = device.nearest_numa_node;
                    return;
                }
            }
            if(device.parent_uuid)
                device.id = AcceleratorId{next_id()};
            else
                for(auto &existing : accelerators.devices)
                    if(existing.parent_uuid && existing.id == device.id)
                        existing.id = AcceleratorId{next_id()};
            accelerators.devices.push_back(std::move(device));
        };
        for(const auto *record : raw.get_all(RawSource::Nvml))
        {
            if(record->status != CollectStatus::Success)
                continue;
            const auto evidence = nlohmann::json::parse(record->payload, nullptr, false);
            if(!evidence.is_object() || !evidence.contains("devices") || !evidence["devices"].is_array())
            {
                warnings.push_back("parse_accelerator: malformed NVML evidence");
                continue;
            }
            // Physical ordinals are inserted first, so synthetic MIG IDs cannot collide with them.
            for(const bool children : {false, true})
                for(const auto &entry : evidence["devices"])
                {
                    if(!entry.is_object() || entry.contains("parent_uuid") != children || !entry.contains("index") ||
                       !entry["index"].is_number_unsigned())
                        continue;
                    AcceleratorDevice device;
                    device.id = AcceleratorId{entry["index"].get<std::uint32_t>()};
                    device.vendor = Vendor{"NVIDIA"};
                    device.kind = AcceleratorKind::Gpu;
                    device.name = DeviceName{string_field(entry, "name", "NVIDIA GPU")};
                    if(entry.contains("uuid") && entry["uuid"].is_string())
                        device.uuid = entry["uuid"].get<std::string>();
                    if(children && entry["parent_uuid"].is_string())
                        device.parent_uuid = entry["parent_uuid"].get<std::string>();
                    if(entry.contains("pci_bus_id") && entry["pci_bus_id"].is_string())
                        device.pci_address = parse_pci_address(entry["pci_bus_id"].get<std::string>());
                    if(entry.contains("memory_total") && entry["memory_total"].is_number_unsigned())
                        device.memory_size = MemorySize{entry["memory_total"].get<std::uint64_t>()};
                    if(entry.contains("gpu_instance_id") && entry["gpu_instance_id"].is_number_unsigned())
                        device.gpu_instance_id = entry["gpu_instance_id"].get<std::uint32_t>();
                    if(entry.contains("compute_instance_id") && entry["compute_instance_id"].is_number_unsigned())
                        device.compute_instance_id = entry["compute_instance_id"].get<std::uint32_t>();
                    merge(std::move(device));
                }
        }
        for(const auto *record : raw.get_all(RawSource::NvidiaSmi))
        {
            if(record->status != CollectStatus::Success)
                continue;
            for(const auto &line : split(record->payload, '\n'))
            {
                if(trim(line).empty() || is_nvidia_smi_header(line))
                    continue;
                if(auto device = parse_nvidia_smi_row(line, warnings))
                    merge(std::move(*device));
            }
        }
        for(const auto *record : raw.get_all(RawSource::NvidiaMigList))
        {
            if(record->status != CollectStatus::Success)
                continue;
            std::optional<std::string> parent;
            for(const auto &line : split(record->payload, '\n'))
            {
                const auto value = trim(line);
                const auto uuid_start = value.find("(UUID: ");
                const auto uuid_end = value.find(')', uuid_start == std::string::npos ? 0 : uuid_start);
                if(uuid_start == std::string::npos || uuid_end == std::string::npos)
                    continue;
                const auto uuid = value.substr(uuid_start + 7, uuid_end - uuid_start - 7);
                if(value.starts_with("GPU "))
                {
                    parent = uuid;
                    const auto colon = value.find(':');
                    const auto index = parse_uint(std::string_view(value).substr(4, colon - 4));
                    for(auto &device : accelerators.devices)
                        if(index && !device.parent_uuid && device.vendor.value == "NVIDIA" &&
                           device.id.value() == *index)
                            device.uuid = uuid;
                }
                else if(value.starts_with("MIG ") && parent)
                {
                    AcceleratorDevice device;
                    device.vendor = Vendor{"NVIDIA"};
                    device.kind = AcceleratorKind::Gpu;
                    device.name = DeviceName{trim(value.substr(0, value.find("Device")))};
                    device.uuid = uuid;
                    device.parent_uuid = parent;
                    for(const auto &physical : accelerators.devices)
                        if(physical.uuid == parent)
                        {
                            device.pci_address = physical.pci_address;
                            device.nearest_numa_node = physical.nearest_numa_node;
                        }
                    merge(std::move(device));
                }
            }
        }
        for(const auto *record : raw.get_all(RawSource::AcceleratorRuntime))
        {
            if(record->status != CollectStatus::Success)
                continue;
            const auto evidence = nlohmann::json::parse(record->payload, nullptr, false);
            if(!evidence.is_object() || !evidence.contains("devices") || !evidence["devices"].is_array())
                continue;
            const auto vendor = string_field(evidence, "vendor");
            if(vendor != "AMD" && vendor != "Intel" && vendor != "NVIDIA")
                continue;
            for(const auto &entry : evidence["devices"])
            {
                if(!entry.is_object())
                    continue;
                const auto pci = parse_pci_address(string_field(entry, "pci_bus_id"));
                if(!pci)
                    continue;
                if(vendor == "NVIDIA")
                {
                    const auto uuid = string_field(entry, "uuid_hex");
                    if(std::any_of(accelerators.devices.begin(), accelerators.devices.end(),
                                   [&](const auto &device) { return device.uuid && uuid_hex(*device.uuid) == uuid; }))
                        continue;
                }
                AcceleratorDevice device;
                device.id = AcceleratorId{next_id()};
                device.vendor = Vendor{vendor};
                device.kind = AcceleratorKind::Gpu;
                device.name = DeviceName{string_field(entry, "name", vendor + " GPU")};
                device.pci_address = pci;
                if(vendor == "NVIDIA")
                {
                    const auto hex = string_field(entry, "uuid_hex");
                    if(hex.size() == 32)
                    {
                        const auto formatted = hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-" + hex.substr(12, 4) +
                                               "-" + hex.substr(16, 4) + "-" + hex.substr(20);
                        device.uuid = "GPU-" + formatted;
                        for(const auto &parent : accelerators.devices)
                            if(!parent.parent_uuid && parent.pci_address == pci && parent.uuid &&
                               uuid_hex(*parent.uuid) != hex)
                            {
                                device.parent_uuid = parent.uuid;
                                device.uuid = "MIG-" + formatted;
                            }
                    }
                }
                if(entry.contains("memory_total") && entry["memory_total"].is_number_unsigned())
                    device.memory_size = MemorySize{entry["memory_total"].get<std::uint64_t>()};
                merge(std::move(device));
            }
        }
        auto drm = raw.get_all(RawSource::SysfsDrm);
        std::sort(drm.begin(), drm.end(),
                  [](const auto *left, const auto *right) { return left->path_or_command < right->path_or_command; });
        for(const auto *record : drm)
        {
            if(record->status != CollectStatus::Success)
                continue;
            const auto entry = nlohmann::json::parse(record->payload, nullptr, false);
            if(!entry.is_object())
            {
                warnings.push_back("parse_accelerator: malformed DRM evidence");
                continue;
            }
            const auto vendor = parse_hex(trim(string_field(entry, "vendor")));
            if(!vendor || (*vendor != 0x1002 && *vendor != 0x8086 && *vendor != 0x10de))
                continue;
            AcceleratorDevice device;
            device.id = AcceleratorId{next_id()};
            device.vendor = Vendor{*vendor == 0x1002 ? "AMD" : (*vendor == 0x8086 ? "Intel" : "NVIDIA")};
            device.kind = AcceleratorKind::Gpu;
            device.name = DeviceName{trim(string_field(
                entry, "product_name", device.vendor.value + " GPU " + trim(string_field(entry, "device"))))};
            device.pci_address = parse_pci_address(string_field(entry, "pci_bus_id"));
            if(const auto size = parse_uint(trim(string_field(entry, "mem_info_vram_total"))))
                device.memory_size = MemorySize{*size};
            if(const auto numa = parse_uint(trim(string_field(entry, "numa_node"))))
                device.nearest_numa_node = NumaNodeId{static_cast<std::uint32_t>(*numa)};
            if(const auto unique = parse_hex(trim(string_field(entry, "unique_id"))); unique && *unique != 0)
            {
                char buffer[32];
                std::snprintf(buffer, sizeof(buffer), "GPU-%016llx", static_cast<unsigned long long>(*unique));
                device.uuid = buffer;
            }
            merge(std::move(device));
        }
        for(auto &device : accelerators.devices)
            if(device.pci_address && !device.nearest_numa_node)
                device.nearest_numa_node = find_numa_node_for_pci(raw, *device.pci_address);
        if(accelerators.devices.empty() && !raw.has_success(RawSource::SysfsDrm) && !raw.has_success(RawSource::Nvml))
        {
            warnings.push_back("parse_accelerator: no readable accelerator backend");
            return std::nullopt;
        }
        return accelerators;
    }

    std::vector<std::pair<AcceleratorId, bool>> parse_accelerator_runtime_visibility(const RawStore &raw,
                                                                                     const Accelerators &devices)
    {
        std::vector<std::pair<AcceleratorId, bool>> result;
        for(const auto *record : raw.get_all(RawSource::AcceleratorRuntime))
        {
            if(record->status != CollectStatus::Success)
                continue;
            const auto evidence = nlohmann::json::parse(record->payload, nullptr, false);
            if(!evidence.is_object() || !evidence.contains("devices") || !evidence["devices"].is_array())
                continue;
            const auto vendor = string_field(evidence, "vendor");
            if(vendor != "AMD" && vendor != "Intel" && vendor != "NVIDIA")
                continue;
            std::vector<PciAddress> addresses;
            std::vector<std::string> uuids;
            bool complete = true;
            for(const auto &entry : evidence["devices"])
            {
                if(!entry.is_object())
                {
                    complete = false;
                    break;
                }
                const auto pci = parse_pci_address(string_field(entry, "pci_bus_id"));
                if(!pci)
                {
                    complete = false;
                    break;
                }
                addresses.push_back(*pci);
                uuids.push_back(string_field(entry, "uuid_hex"));
            }
            if(!complete)
                continue;
            std::vector<AcceleratorId> visible;
            for(std::size_t index = 0; index < addresses.size(); ++index)
            {
                bool matched = false;
                for(const auto &device : devices.devices)
                {
                    if(device.vendor.value != vendor)
                        continue;
                    const bool match = vendor == "NVIDIA"
                                           ? device.uuid && uuid_hex(*device.uuid) == uuids[index]
                                           : device.pci_address == std::optional<PciAddress>{addresses[index]};
                    if(match)
                    {
                        visible.push_back(device.id);
                        matched = true;
                    }
                }
                if(!matched)
                {
                    complete = false;
                    break;
                }
            }
            if(!complete)
                continue; // Missing UUID/BDF mappings cannot establish a complete exclusion set.
            for(const auto id : visible)
                result.emplace_back(id, true);
            for(const auto &device : devices.devices)
                if(device.vendor.value == vendor &&
                   std::find(visible.begin(), visible.end(), device.id) == visible.end())
                    result.emplace_back(device.id, false);
        }
        return result;
    }

} // namespace sysal::detail
