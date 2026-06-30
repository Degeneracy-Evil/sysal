/// @file accelerator_parser.cpp
/// @brief 加速器子系统解析器实现
/// @details 解析 nvidia-smi CSV 格式输出，构建 GPU 加速器设备列表。
///          CSV 列顺序：index, name, memory(MiB), pci_address, driver_version。

#include "accelerator_parser.hpp"
#include "parse_utils.hpp"
#include "parsed_facts.hpp"

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <string>

namespace sysal::detail
{

namespace
{

/// @brief 判断某条采集记录是否为 nvidia-smi CSV 输出
/// @param path_or_command 记录的路径或命令字段
/// @return 以 "nvidia-smi" 开头时返回 true
bool is_nvidia_smi_csv(const std::string& path_or_command)
{
    return path_or_command.starts_with("nvidia-smi");
}

/// @brief 将形如 "24576 MiB" 的字段解析为字节数
/// @param field 含数值与单位的内存字段
/// @return 以字节为单位的 MemorySize；无法解析时返回 std::nullopt
/// @details 仅取第一个以空格分隔的字段作为 MiB 数值，再换算为字节（×1024×1024）。
std::optional<MemorySize> parse_memory_mib(const std::string& field)
{
    auto parts = split(field, ' ');
    if(parts.empty())
    {
        return std::nullopt;
    }
    auto mib = parse_uint(parts[0]);
    if(!mib)
    {
        return std::nullopt;
    }
    // MiB 换算为字节
    return MemorySize{*mib * 1024U * 1024U};
}

/// @brief 解析一行 nvidia-smi CSV 数据为 AcceleratorDevice
/// @param line 单行 CSV 文本
/// @return 解析成功返回 AcceleratorDevice；字段不足或 index 无效时返回 std::nullopt
/// @details 期望至少 5 列，依次为：
///          - fields[0]：设备索引；
///          - fields[1]：设备名；
///          - fields[2]：显存（MiB）；
///          - fields[3]：PCI 地址；
///          - fields[4]：驱动版本（此处不使用）。
///          所有字段会先做 trim。设备默认标记为对当前进程可见。
std::optional<AcceleratorDevice> parse_csv_line(const std::string& line)
{
    auto fields = split(line, ',');
    // 列数不足，无法解析
    if(fields.size() < 5)
    {
        return std::nullopt;
    }

    // 去除每个字段两侧空白
    for(auto& f : fields)
    {
        f = trim(f);
    }

    // 解析设备索引，无效则放弃该行
    auto index = parse_uint(fields[0]);
    if(!index)
    {
        return std::nullopt;
    }

    auto memory = parse_memory_mib(fields[2]);
    auto pci = parse_pci_address(fields[3]);

    AcceleratorDevice device{};
    device.id = AcceleratorId{static_cast<std::uint32_t>(*index)};
    device.kind = AcceleratorKind::Gpu;
    device.vendor = Vendor{"NVIDIA"};
    device.name = DeviceName{fields[1]};
    device.pci_address = pci;
    device.memory_size = memory;
    device.visible_to_current_process = true;
    return device;
}

} // namespace

/// @brief 从 RawStore 解析加速器子系统信息
/// @param raw 原始数据存储
/// @param diag 诊断信息容器，用于记录解析过程中的告警
/// @return 解析出至少一个设备时返回 AcceleratorSubsystem；否则返回 std::nullopt
/// @details 遍历所有 RawSource::NvidiaSmi 采集成功且为 nvidia-smi 命令的记录，
///          逐行解析 CSV。解析失败的行会产生告警但不会中断整体流程。
std::optional<AcceleratorSubsystem> parse_accelerators(const RawStore& raw, Diagnostics& diag)
{
    AcceleratorSubsystem facts;
    bool found_any = false;

    for(const auto& record : raw.records)
    {
        // 仅处理采集成功的 nvidia-smi 记录
        if(record.source != RawSource::NvidiaSmi || record.status != CollectStatus::Success)
        {
            continue;
        }
        if(!is_nvidia_smi_csv(record.path_or_command))
        {
            continue;
        }

        auto lines = split(record.payload, '\n');
        for(const auto& line : lines)
        {
            auto trimmed = trim(line);
            if(trimmed.empty())
            {
                continue;
            }
            auto device = parse_csv_line(trimmed);
            if(!device)
            {
                // 单行解析失败：记录告警并跳过该设备
                add_warning(diag, "Failed to parse nvidia-smi CSV line: " + trimmed,
                            RawSource::NvidiaSmi);
                continue;
            }
            facts.devices.push_back(*device);
            found_any = true;
        }
    }

    // 一个设备都没解析到则视为无加速器
    if(!found_any)
    {
        return std::nullopt;
    }

    return facts;
}

} // namespace sysal::detail
