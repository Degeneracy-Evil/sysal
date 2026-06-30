/// @file accelerator.cpp
/// @brief 加速器解析器实现
/// @details 从 nvidia-smi 命令输出和 sysfs PCI 数据中解析 GPU 等加速器信息。

#include "accelerator.hpp"

#include "parse_utils.hpp"

#include <string_view>

namespace sysal::detail
{

namespace
{

/// @brief 解析 nvidia-smi CSV 输出中的单行
/// @param line CSV 行内容（格式: index, name, pci.bus_id, memory.total）
/// @param warnings 警告列表
/// @return 解析成功返回 AcceleratorDevice，否则返回 nullopt
std::optional<AcceleratorDevice> parse_nvidia_smi_row(std::string_view line,
                                                      std::vector<std::string>& warnings)
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

    // 字段 2: pci.bus_id
    auto pci = parse_pci_address(trim(fields[2]));
    if(pci.has_value())
    {
        dev.pci_address = *pci;
    }
    else
    {
        warnings.push_back("parse_accelerator: nvidia-smi pci.bus_id 解析失败: " + trim(fields[2]));
    }

    // 字段 3: memory.total（格式: "97536 MiB"）
    {
        auto mem_str = trim(fields[3]);
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
                    if(unit == "MiB")
                    {
                        multiplier = 1024ULL * 1024ULL;
                    }
                    else if(unit == "GiB")
                    {
                        multiplier = 1024ULL * 1024ULL * 1024ULL;
                    }
                    else if(unit == "KiB")
                    {
                        multiplier = 1024ULL;
                    }
                    else if(unit == "B")
                    {
                        multiplier = 1ULL;
                    }
                    else
                    {
                        // 未知单位，默认按 MiB 处理
                        warnings.push_back("parse_accelerator: nvidia-smi memory.total 未知单位: " +
                                           unit + "，默认按 MiB 处理");
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

    return dev;
}

/// @brief 从 SysfsPci 记录中查找指定 PCI 地址的 NUMA 节点
/// @param raw 原始证据存储
/// @param pci_addr PCI 地址
/// @return NUMA 节点 ID，未找到则返回 nullopt
std::optional<NumaNodeId> find_numa_node_for_pci(const RawStore& raw, const PciAddress& pci_addr)
{
    // 构建 PCI 地址字符串用于匹配 sysfs 路径
    // sysfs 路径格式: /sys/bus/pci/devices/DDDD:BB:DD.F/numa_node
    // PCI 地址格式: DDDD:BB:DD.F（十六进制）
    char addr_buf[32];
    std::snprintf(addr_buf, sizeof(addr_buf), "%04x:%02x:%02x.%x",
                  static_cast<unsigned>(pci_addr.domain), static_cast<unsigned>(pci_addr.bus),
                  static_cast<unsigned>(pci_addr.device), static_cast<unsigned>(pci_addr.function));

    auto pci_records = raw.get_all(RawSource::SysfsPci);
    for(const auto* rec : pci_records)
    {
        const auto& path = rec->path_or_command;
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
    return trimmed.find("index") != std::string_view::npos ||
           trimmed.find("name") != std::string_view::npos;
}

} // namespace

std::optional<Accelerators> parse_accelerator(const RawStore& raw,
                                              std::vector<std::string>& warnings)
{
    // 获取 nvidia-smi 记录
    auto nvidia_records = raw.get_all(RawSource::NvidiaSmi);
    if(nvidia_records.empty())
    {
        warnings.push_back("parse_accelerator: 缺少 nvidia-smi 数据");
        return std::nullopt;
    }

    Accelerators accelerators;

    for(const auto* rec : nvidia_records)
    {
        auto lines = split(rec->payload, '\n');
        for(const auto& line : lines)
        {
            auto trimmed = trim(line);
            if(trimmed.empty() || is_nvidia_smi_header(trimmed))
            {
                continue;
            }

            auto dev = parse_nvidia_smi_row(trimmed, warnings);
            if(dev.has_value())
            {
                // D-4 修正：从 SysfsPci 查找 NUMA 节点
                if(dev->pci_address.has_value())
                {
                    auto numa = find_numa_node_for_pci(raw, *dev->pci_address);
                    if(numa.has_value())
                    {
                        dev->nearest_numa_node = *numa;
                    }
                }

                accelerators.devices.push_back(std::move(*dev));
            }
        }
    }

    if(accelerators.devices.empty())
    {
        warnings.push_back("parse_accelerator: nvidia-smi 输出中无有效 GPU 条目");
        return std::nullopt;
    }

    return accelerators;
}

} // namespace sysal::detail
