#include "software_parser.hpp"
#include "parse_utils.hpp"
#include "parsed_facts.hpp"

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <string>

namespace sysal::detail
{

namespace
{

bool is_version_token(const std::string& token)
{
    if(token.empty())
    {
        return false;
    }
    bool has_digit = false;
    for(char c : token)
    {
        if(c >= '0' && c <= '9')
        {
            has_digit = true;
        }
        else if(c != '.')
        {
            return false;
        }
    }
    return has_digit;
}

std::optional<std::string> extract_nvrm_driver_version(const std::string& payload)
{
    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto trimmed = trim(line);
        if(!trimmed.starts_with("NVRM version"))
        {
            continue;
        }
        auto parts = split(trimmed, ' ');
        for(const auto& part : parts)
        {
            if(is_version_token(part))
            {
                return part;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> extract_cuda_release_version(const std::string& payload)
{
    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto trimmed = trim(line);
        auto pos = trimmed.find("release ");
        if(pos == std::string::npos)
        {
            continue;
        }
        auto rest = std::string_view(trimmed).substr(pos + 8);
        auto comma = rest.find(',');
        if(comma == std::string_view::npos)
        {
            continue;
        }
        auto version = trim(rest.substr(0, comma));
        if(!version.empty())
        {
            return version;
        }
    }
    return std::nullopt;
}

std::optional<std::string> extract_smi_driver_version(const std::string& payload)
{
    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto trimmed = trim(line);
        if(trimmed.empty())
        {
            continue;
        }
        auto fields = split(trimmed, ',');
        if(fields.size() < 5)
        {
            continue;
        }
        auto version = trim(fields.back());
        if(!version.empty())
        {
            return version;
        }
    }
    return std::nullopt;
}

std::uint32_t count_smi_devices(const std::string& payload)
{
    std::uint32_t count = 0;
    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto trimmed = trim(line);
        if(trimmed.empty())
        {
            continue;
        }
        auto fields = split(trimmed, ',');
        if(fields.size() >= 5)
        {
            ++count;
        }
    }
    return count;
}

} // namespace

std::optional<SoftwareStackInfo> parse_software_stack(const RawStore& raw, Diagnostics& diag)
{
    std::optional<std::string> driver_version;
    std::optional<std::string> cuda_version;
    std::uint32_t device_count = 0;

    for(const auto& record : raw.records)
    {
        if(record.source != RawSource::NvidiaSmi)
        {
            continue;
        }
        if(record.status != CollectStatus::Success)
        {
            add_warning(diag, "NVIDIA data collection failed for: " + record.path_or_command,
                        RawSource::NvidiaSmi);
            continue;
        }

        if(record.path_or_command == "/proc/driver/nvidia/version")
        {
            auto v = extract_nvrm_driver_version(record.payload);
            if(v && !driver_version)
            {
                driver_version = v;
            }
        }
        else if(record.path_or_command == "nvcc --version")
        {
            auto v = extract_cuda_release_version(record.payload);
            if(v && !cuda_version)
            {
                cuda_version = v;
            }
        }
        else if(record.path_or_command.starts_with("nvidia-smi"))
        {
            device_count += count_smi_devices(record.payload);
            auto v = extract_smi_driver_version(record.payload);
            if(v && !driver_version)
            {
                driver_version = v;
            }
        }
    }

    if(!driver_version && !cuda_version)
    {
        return std::nullopt;
    }

    SoftwareStackInfo info{};

    if(driver_version)
    {
        info.drivers.push_back({
            .name = "nvidia",
            .version = *driver_version,
            .loaded = true,
        });
    }

    if(cuda_version)
    {
        info.runtimes.push_back({
            .name = "cuda",
            .version = *cuda_version,
            .path = "",
        });
    }

    info.cuda = CudaInfo{
        .driver_version = driver_version.value_or(""),
        .runtime_version = cuda_version.value_or(""),
        .device_count = device_count,
    };

    return info;
}

} // namespace sysal::detail
