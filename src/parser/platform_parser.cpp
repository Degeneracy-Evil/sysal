/// @file platform_parser.cpp
/// @brief 平台信息解析器实现
/// @details 从 /etc/os-release、/proc/version、uname 输出解析操作系统、内核与架构
///          信息，并通过 gethostname 获取主机名。

#include "platform_parser.hpp"
#include "parse_utils.hpp"
#include "parsed_facts.hpp"

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <unistd.h>

#include <array>
#include <string>

namespace sysal::detail
{

namespace
{

/// @brief 从 /etc/os-release 原始记录解析操作系统名称与版本
/// @param raw 原始数据存储
/// @param facts 待填充的平台信息（仅更新 os 字段）
void parse_os_release(const RawStore& raw, PlatformInfo& facts)
{
    auto records = raw.get_all(RawSource::ProcUname);
    for(const auto* record : records)
    {
        if(record->path_or_command != "/etc/os-release")
        {
            continue;
        }
        auto lines = split(record->payload, '\n');
        for(const auto& line : lines)
        {
            // os-release 以 '=' 分隔键值，值可能带引号需 unquote
            auto [key, value] = split_kv(line, '=');
            if(key == "NAME")
            {
                facts.os.name = unquote(value);
            }
            else if(key == "VERSION")
            {
                facts.os.version = unquote(value);
            }
        }
    }
}

/// @brief 从 /proc/version 解析内核版本信息
/// @param raw 原始数据存储
/// @param facts 待填充的平台信息（更新 kernel.version 与 kernel.release）
void parse_version(const RawStore& raw, PlatformInfo& facts)
{
    auto records = raw.get_all(RawSource::ProcVersion);
    if(records.empty())
    {
        return;
    }
    facts.kernel.version = trim(records[0]->payload);
    // /proc/version 格式: "Linux version <release> ..."，release 为第三段
    auto parts = split(records[0]->payload, ' ');
    if(parts.size() >= 3)
    {
        facts.kernel.release = parts[2];
    }
}

/// @brief 从 uname 原始记录解析机器架构
/// @param raw 原始数据存储
/// @param facts 待填充的平台信息（更新 architecture 字段）
void parse_uname(const RawStore& raw, PlatformInfo& facts)
{
    auto records = raw.get_all(RawSource::ProcUname);
    for(const auto* record : records)
    {
        if(record->path_or_command != "uname")
        {
            continue;
        }
        auto parts = split(record->payload, ' ');
        if(!parts.empty())
        {
            // uname 最后一字段为 machine 架构标识（如 x86_64、aarch64）
            facts.architecture.machine_arch = parts.back();
            facts.architecture.cpu_arch = arch_from_machine(parts.back());
        }
    }
}

/// @brief 通过 gethostname 系统调用获取主机名
/// @param facts 待填充的平台信息（更新 host.hostname）
void parse_hostname(PlatformInfo& facts)
{
    std::array<char, 256> hostname{};
    if(gethostname(hostname.data(), hostname.size()) == 0)
    {
        facts.host.hostname = std::string(hostname.data());
    }
}

} // namespace

std::optional<PlatformInfo> parse_platform(const RawStore& raw, Diagnostics& diag)
{
    PlatformInfo facts;

    parse_hostname(facts);
    parse_os_release(raw, facts);
    parse_version(raw, facts);
    parse_uname(raw, facts);

    // 无法从 uname 确定架构时记录警告（不影响其它字段的可用性）
    if(facts.architecture.machine_arch.empty())
    {
        add_warning(diag, "Could not determine machine architecture from uname",
                    RawSource::ProcUname);
    }

    return facts;
}

} // namespace sysal::detail
