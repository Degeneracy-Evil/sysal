/// @file platform_info.hpp
/// @brief 平台信息数据模型
/// @details 描述主机、操作系统、内核、架构、固件与虚拟化环境等平台层信息。

#pragma once

#include "sysal/enums.hpp"
#include "sysal/value_types.hpp"

#include <optional>
#include <string>

namespace sysal
{

/// @brief 主机信息
struct HostInfo
{
    std::string hostname; ///< 主机名
};

/// @brief 操作系统信息
struct OsInfo
{
    std::string name;    ///< 操作系统名称
    std::string version; ///< 操作系统版本
};

/// @brief 内核信息
struct KernelInfo
{
    std::string version; ///< 内核版本
    std::string release; ///< 内核发行号
};

/// @brief 架构信息
struct ArchitectureInfo
{
    Architecture cpu_arch;    ///< CPU 架构枚举
    std::string machine_arch; ///< 机器架构字符串（如 x86_64、aarch64）
};

/// @brief 固件（BIOS）信息
struct FirmwareInfo
{
    std::string bios_version; ///< BIOS 版本
    std::string bios_vendor;  ///< BIOS 厂商
    std::string bios_date;    ///< BIOS 发布日期
};

/// @brief 虚拟化环境信息
struct VirtualizationInfo
{
    VirtualizationKind kind; ///< 虚拟化类型
    std::string hypervisor;  ///< 虚拟机监控程序名称
};

/// @brief 平台信息聚合
/// @details 汇总主机、操作系统、内核、架构、固件与虚拟化等平台层信息。
struct PlatformInfo
{
    HostInfo host;                                    ///< 主机信息
    OsInfo os;                                        ///< 操作系统信息
    KernelInfo kernel;                                ///< 内核信息
    ArchitectureInfo architecture;                    ///< 架构信息
    std::optional<FirmwareInfo> firmware;             ///< 固件信息（可能不可用）
    std::optional<VirtualizationInfo> virtualization; ///< 虚拟化信息（可能不可用）
};

} // namespace sysal
