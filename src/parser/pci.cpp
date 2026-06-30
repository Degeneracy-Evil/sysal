#include "pci.hpp"

#include "parse_utils.hpp"

#include <map>
#include <string_view>

namespace sysal::detail
{

namespace
{

/// @brief 从 sysfs 路径提取 PCI 地址
/// @param path sysfs 路径，如 "/sys/bus/pci/devices/0000:41:00.0/vendor"
/// @return PCI 地址字符串（路径最后一段），如 "0000:41:00.0"
std::string extract_pci_address_from_path(std::string_view path)
{
    // 路径格式: /sys/bus/pci/devices/DDDDD:BB:DD.F/<file>
    // 或 /sys/bus/pci/devices/DDDDD:BB:DD.F (目录本身)
    // 需要提取 DDDDD:BB:DD.F 部分
    auto last_slash = path.rfind('/');
    if(last_slash == std::string_view::npos)
    {
        return std::string(path);
    }
    auto after_slash = path.substr(last_slash + 1);

    // 如果 after_slash 是文件名（vendor, device, class, numa_node），
    // 则需要再往前取一段
    if(after_slash == "vendor" || after_slash == "device" || after_slash == "class" ||
       after_slash == "numa_node")
    {
        auto rest = path.substr(0, last_slash);
        auto prev_slash = rest.rfind('/');
        if(prev_slash == std::string_view::npos)
        {
            return std::string(rest);
        }
        return std::string(rest.substr(prev_slash + 1));
    }

    return std::string(after_slash);
}

/// @brief 从 sysfs 路径提取文件名
/// @param path sysfs 路径，如 "/sys/bus/pci/devices/0000:41:00.0/vendor"
/// @return 文件名，如 "vendor"
std::string extract_filename_from_path(std::string_view path)
{
    auto last_slash = path.rfind('/');
    if(last_slash == std::string_view::npos)
    {
        return std::string(path);
    }
    return std::string(path.substr(last_slash + 1));
}

} // namespace

std::optional<Pci> parse_pci(const RawStore& raw, std::vector<std::string>& warnings)
{
    auto pci_records = raw.get_all(RawSource::SysfsPci);
    if(pci_records.empty())
    {
        return std::nullopt;
    }

    // 按 PCI 地址分组
    std::map<std::string, std::vector<const RawRecord*>> groups;
    for(const auto* rec : pci_records)
    {
        auto addr_str = extract_pci_address_from_path(rec->path_or_command);
        groups[addr_str].push_back(rec);
    }

    Pci pci;
    for(const auto& [addr_str, records] : groups)
    {
        PciDevice dev;

        // 解析 PCI 地址
        auto addr = parse_pci_address(addr_str);
        if(!addr.has_value())
        {
            warnings.push_back("parse_pci: 无法解析 PCI 地址: " + addr_str);
            continue;
        }
        dev.address = *addr;

        // 从分组记录中提取各字段
        for(const auto* rec : records)
        {
            auto filename = extract_filename_from_path(rec->path_or_command);
            const auto& payload = rec->payload;

            if(filename == "vendor")
            {
                // hex 字符串如 "0x10de"，保留原始值作为 NamedString
                auto trimmed = trim(payload);
                dev.vendor = Vendor{trimmed};
            }
            else if(filename == "device")
            {
                auto trimmed = trim(payload);
                dev.device_name = DeviceName{trimmed};
            }
            else if(filename == "class")
            {
                auto trimmed = trim(payload);
                dev.device_class = PciClass{trimmed};
            }
            else if(filename == "numa_node")
            {
                auto trimmed = trim(payload);
                auto val = parse_uint(trimmed);
                if(val.has_value())
                {
                    if(static_cast<std::int64_t>(*val) == -1)
                    {
                        // -1 表示无 NUMA 归属
                        dev.numa_node = std::nullopt;
                    }
                    else
                    {
                        dev.numa_node = NumaNodeId{static_cast<std::uint32_t>(*val)};
                    }
                }
                else
                {
                    // parse_uint 无法解析负数，尝试直接检查 "-1"
                    if(trimmed == "-1")
                    {
                        dev.numa_node = std::nullopt;
                    }
                    else
                    {
                        warnings.push_back("parse_pci: numa_node 值解析失败: " + trimmed);
                    }
                }
            }
        }

        pci.devices.push_back(dev);
    }

    return pci;
}

} // namespace sysal::detail
