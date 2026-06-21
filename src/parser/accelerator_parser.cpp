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

bool is_nvidia_smi_csv(const std::string& path_or_command)
{
    return path_or_command.starts_with("nvidia-smi");
}

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
    return MemorySize{*mib * 1024U * 1024U};
}

std::optional<AcceleratorDevice> parse_csv_line(const std::string& line)
{
    auto fields = split(line, ',');
    if(fields.size() < 5)
    {
        return std::nullopt;
    }

    for(auto& f : fields)
    {
        f = trim(f);
    }

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

std::optional<AcceleratorSubsystem> parse_accelerators(const RawStore& raw, Diagnostics& diag)
{
    AcceleratorSubsystem facts;
    bool found_any = false;

    for(const auto& record : raw.records)
    {
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
                add_warning(diag, "Failed to parse nvidia-smi CSV line: " + trimmed,
                            RawSource::NvidiaSmi);
                continue;
            }
            facts.devices.push_back(*device);
            found_any = true;
        }
    }

    if(!found_any)
    {
        return std::nullopt;
    }

    return facts;
}

} // namespace sysal::detail
