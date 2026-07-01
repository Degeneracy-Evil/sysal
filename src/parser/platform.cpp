/// @file platform.cpp
/// @brief 平台信息解析器实现
/// @details 从 /proc 和 sysfs 解析主机名、OS、内核、架构、固件和虚拟化信息。

#include "platform.hpp"

#include "parse_utils.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace sysal::detail
{

namespace
{

/// @brief 解析 /etc/os-release 内容，填充 Os 结构体
/// @param payload /etc/os-release 文件内容
/// @param warnings 警告列表
/// @return Os 结构体
Os parse_os_release(std::string_view payload, [[maybe_unused]] std::vector<std::string>& warnings)
{
    Os os;
    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto [key, value] = parse_kv(line, '=');
        // 去除值两端可能的引号
        if(value.size() >= 2 && value.front() == '"' && value.back() == '"')
        {
            value = value.substr(1, value.size() - 2);
        }
        if(key == "NAME")
        {
            os.name = value;
        }
        else if(key == "VERSION_ID")
        {
            os.version = value;
        }
        else if(key == "ID")
        {
            os.distribution = value;
        }
        else if(key == "VERSION_CODENAME")
        {
            os.codename = value;
        }
        else if(key == "VERSION")
        {
            // VERSION 字段可能包含 "16.04.1 LTS (Xenial Xerus)" 格式
            // 提取 distribution_version
            os.distribution_version = value;
        }
    }
    return os;
}

/// @brief 解析 /proc/version 内容，填充 Kernel 结构体
/// @param payload /proc/version 文件内容
/// @param warnings 警告列表
/// @return Kernel 结构体
Kernel parse_proc_version(std::string_view payload, std::vector<std::string>& warnings)
{
    Kernel kernel;
    // 格式: Linux version 5.15.0-91-generic (buildd@...) (gcc ...) #101-Ubuntu SMP ...
    auto trimmed = trim(payload);
    if(trimmed.empty())
    {
        warnings.push_back("parse_platform: /proc/version 内容为空");
        return kernel;
    }

    // 提取内核发行号：在 "Linux version " 之后，空格之前
    const std::string_view prefix = "Linux version ";
    auto pos = trimmed.find(prefix);
    if(pos != std::string_view::npos)
    {
        auto rest = trimmed.substr(pos + prefix.size());
        auto space = rest.find(' ');
        if(space != std::string_view::npos)
        {
            kernel.release = std::string(rest.substr(0, space));
        }
        else
        {
            kernel.release = std::string(rest);
        }
    }
    else
    {
        warnings.push_back("parse_platform: /proc/version 格式异常，无法提取内核发行号");
    }

    // version: 从 release 截取上游版本号（"-" 之前的部分，如 "6.8.0"）
    if(!kernel.release.empty())
    {
        auto dash = kernel.release.find('-');
        kernel.version = (dash != std::string::npos)
                             ? kernel.release.substr(0, dash)
                             : kernel.release;
    }

    // compiled_at: 从 # 后提取编译时间戳（以星期几开头）
    auto hash_pos = trimmed.rfind('#');
    if(hash_pos != std::string_view::npos)
    {
        auto after_hash = trimmed.substr(hash_pos);
        static const std::array<std::string_view, 7> days = {
            "Mon ", "Tue ", "Wed ", "Thu ", "Fri ", "Sat ", "Sun "};

        auto ts_pos = std::string_view::npos;
        for(auto day : days)
        {
            auto p = after_hash.find(day);
            if(p != std::string_view::npos && (ts_pos == std::string_view::npos || p < ts_pos))
            {
                ts_pos = p;
            }
        }
        if(ts_pos != std::string_view::npos)
        {
            kernel.compiled_at = std::string(after_hash.substr(ts_pos));
        }
    }
    return kernel;
}

/// @brief 从 uname -m 输出解析架构
/// @param payload uname -m 输出
/// @return 架构名称字符串
std::string parse_arch_name(std::string_view payload) { return trim(std::string(payload)); }

/// @brief 从架构名称推断位宽
/// @param name 架构名称
/// @return 位宽（32 或 64）
std::uint32_t infer_bits(const std::string& name)
{
    if(name.find("64") != std::string::npos)
    {
        return 64;
    }
    return 32;
}

/// @brief 解析 DMI 信息，填充 Firmware 和 Host 字段
/// @param raw 原始证据存储
/// @param platform Platform 结构体（可修改）
/// @param warnings 警告列表
void parse_dmi(const RawStore& raw, Platform& platform,
               [[maybe_unused]] std::vector<std::string>& warnings)
{
    auto dmi_records = raw.get_all(RawSource::SysfsDmi);
    if(dmi_records.empty())
    {
        return;
    }

    Firmware firmware;
    bool has_firmware = false;

    for(const auto* rec : dmi_records)
    {
        if(rec->status != CollectStatus::Success)
        {
            continue;
        }

        const auto& path = rec->path_or_command;
        const auto& payload = rec->payload;

        // BIOS 信息
        if(path.find("bios_vendor") != std::string::npos)
        {
            firmware.bios_vendor = Vendor{trim(payload)};
            has_firmware = true;
        }
        else if(path.find("bios_version") != std::string::npos)
        {
            firmware.bios_version = trim(payload);
            has_firmware = true;
        }
        else if(path.find("bios_date") != std::string::npos)
        {
            firmware.bios_date = trim(payload);
            has_firmware = true;
        }
        // 主机信息
        else if(path.find("product_name") != std::string::npos)
        {
            platform.host.product_name = trim(payload);
        }
        else if(path.find("sys_vendor") != std::string::npos)
        {
            platform.host.vendor = Vendor{trim(payload)};
        }
        else if(path.find("product_serial") != std::string::npos)
        {
            platform.host.serial = trim(payload);
        }
        else if(path.find("/sys/firmware/efi") != std::string::npos)
        {
            firmware.uefi = true;
            has_firmware = true;
        }
    }

    if(has_firmware)
    {
        platform.firmware = firmware;
    }
}

/// @brief 检测硬件虚拟化信息
/// @param raw 原始证据存储
/// @param warnings 警告列表
/// @return 虚拟化信息（若检测到）
/// @details 仅检测硬件虚拟化（KVM/Xen/VMware），容器信息由 ExecutionContext.container 承载。
std::optional<Virtualization>
detect_virtualization(const RawStore& raw, [[maybe_unused]] std::vector<std::string>& warnings)
{
    Virtualization virt;
    bool detected = false;

    // 检查 /proc/1/cgroup 内容中的硬件虚拟化线索
    auto cgroup_records = raw.get_all(RawSource::ProcOneCgroup);
    for(const auto* rec : cgroup_records)
    {
        if(rec->status != CollectStatus::Success)
        {
            continue;
        }

        const auto& content = rec->payload;
        if(content.find("kvm") != std::string::npos)
        {
            virt.kind = VirtualizationKind::Kvm;
            virt.hypervisor = "KVM";
            detected = true;
        }
    }

    if(detected)
    {
        return virt;
    }
    return std::nullopt;
}

} // namespace

std::optional<Platform> parse_platform(const RawStore& raw, std::vector<std::string>& warnings)
{
    Platform platform;

    // 解析 /etc/os-release → Os
    auto os_release = raw.get_all(RawSource::EtcOsRelease);
    const RawRecord* os_rec = nullptr;
    for(const auto* rec : os_release)
    {
        if(rec->status == CollectStatus::Success)
        {
            os_rec = rec;
            break;
        }
    }
    if(os_rec != nullptr)
    {
        platform.os = parse_os_release(os_rec->payload, warnings);
    }
    else
    {
        warnings.push_back("parse_platform: 缺少 /etc/os-release 数据");
    }

    // 解析 /proc/version → Kernel
    auto proc_version = raw.get_all(RawSource::ProcVersion);
    const RawRecord* ver_rec = nullptr;
    for(const auto* rec : proc_version)
    {
        if(rec->status == CollectStatus::Success)
        {
            ver_rec = rec;
            break;
        }
    }
    if(ver_rec != nullptr)
    {
        platform.kernel = parse_proc_version(ver_rec->payload, warnings);
    }
    else
    {
        warnings.push_back("parse_platform: 缺少 /proc/version 数据");
    }

    // 解析 uname -m → Architecture
    auto uname_records = raw.get_all(RawSource::Uname);
    const RawRecord* uname_rec = nullptr;
    for(const auto* rec : uname_records)
    {
        if(rec->status == CollectStatus::Success)
        {
            uname_rec = rec;
            break;
        }
    }
    if(uname_rec != nullptr)
    {
        auto arch_name = parse_arch_name(uname_rec->payload);
        platform.architecture.name = arch_name;
        platform.architecture.bits = infer_bits(arch_name);
        platform.architecture.byte_order = "little"; // x86_64/aarch64/riscv64 均为小端
        platform.kernel.architecture = arch_name;
    }
    else
    {
        warnings.push_back("parse_platform: 缺少 uname 数据");
    }

    // 解析 DMI → Firmware + Host
    parse_dmi(raw, platform, warnings);

    // 解析主机名
    auto hostname_records = raw.get_all(RawSource::ProcHostname);
    for(const auto* rec : hostname_records)
    {
        if(rec->status == CollectStatus::Success && !rec->payload.empty())
        {
            platform.host.hostname = trim(rec->payload);
            break;
        }
    }

    // 检测虚拟化
    platform.virtualization = detect_virtualization(raw, warnings);

    return platform;
}

} // namespace sysal::detail
