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

void parse_version(const RawStore& raw, PlatformInfo& facts)
{
    auto records = raw.get_all(RawSource::ProcVersion);
    if(records.empty())
    {
        return;
    }
    facts.kernel.version = trim(records[0]->payload);
    auto parts = split(records[0]->payload, ' ');
    if(parts.size() >= 3)
    {
        facts.kernel.release = parts[2];
    }
}

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
            facts.architecture.machine_arch = parts.back();
            facts.architecture.cpu_arch = arch_from_machine(parts.back());
        }
    }
}

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

    if(facts.architecture.machine_arch.empty())
    {
        add_warning(diag, "Could not determine machine architecture from uname",
                    RawSource::ProcUname);
    }

    return facts;
}

} // namespace sysal::detail
