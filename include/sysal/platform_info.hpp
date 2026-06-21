#pragma once

#include "sysal/enums.hpp"
#include "sysal/value_types.hpp"

#include <optional>
#include <string>

namespace sysal
{

struct HostInfo
{
    std::string hostname;
};

struct OsInfo
{
    std::string name;
    std::string version;
};

struct KernelInfo
{
    std::string version;
    std::string release;
};

struct ArchitectureInfo
{
    Architecture cpu_arch;
    std::string machine_arch;
};

struct FirmwareInfo
{
    std::string bios_version;
    std::string bios_vendor;
    std::string bios_date;
};

struct VirtualizationInfo
{
    VirtualizationKind kind;
    std::string hypervisor;
};

struct PlatformInfo
{
    HostInfo host;
    OsInfo os;
    KernelInfo kernel;
    ArchitectureInfo architecture;
    std::optional<FirmwareInfo> firmware;
    std::optional<VirtualizationInfo> virtualization;
};

} // namespace sysal
