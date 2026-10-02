/// @file platform.hpp
/// @brief 平台信息数据模型
/// @details 定义 Platform 及其子结构体（Host、Os、Kernel、Architecture、
///          Baseboard、Chassis、Firmware、Virtualization），描述系统的基本标识。

#pragma once

#include "sysal/types/enums.hpp"
#include "sysal/types/value_types.hpp"

#include <optional>
#include <string>

namespace sysal
{

    /// @brief 主机标识
    struct Host
    {
        std::string hostname;          ///< 主机名
        std::string machine_id;        ///< machine-id
        std::string product_name;      ///< 产品名称
        Vendor vendor;                 ///< 厂商
        std::string serial;            ///< 序列号
        std::string product_family{};  ///< 产品系列
        std::string product_version{}; ///< 产品版本
        std::string product_sku{};     ///< 产品 SKU
        std::string product_uuid{};    ///< 固件产品 UUID，不是 machine-id
    };

    /// @brief 主板标识，由固件提供
    struct Baseboard
    {
        Vendor vendor;
        std::string name;
        std::string version;
        std::string serial;
        std::string asset_tag;
    };

    /// @brief 机箱标识，由固件提供
    struct Chassis
    {
        Vendor vendor;
        std::optional<std::uint32_t> type; ///< SMBIOS 类型编号，保留未知的未来编号
        std::string version;
        std::string serial;
        std::string asset_tag;
    };

    /// @brief 操作系统
    struct Os
    {
        std::string name;                 ///< 操作系统名称
        std::string version;              ///< 操作系统版本
        std::string distribution;         ///< 发行版名称
        std::string distribution_version; ///< 发行版版本
        std::string codename;             ///< 发行版代号
    };

    /// @brief 内核
    struct Kernel
    {
        std::string release;      ///< 内核发行号
        std::string version;      ///< 内核版本
        std::string compiled_at;  ///< 编译时间
        std::string architecture; ///< 内核架构
    };

    /// @brief 硬件架构
    struct Architecture
    {
        std::string name;       ///< 架构名称（如 x86_64、aarch64）
        std::uint32_t bits{};   ///< 位宽（64 或 32）
        std::string byte_order; ///< 字节序（如 little、big）
    };

    /// @brief 固件
    struct Firmware
    {
        Vendor bios_vendor;                ///< BIOS 厂商
        std::string bios_version;          ///< BIOS 版本
        std::string bios_date;             ///< BIOS 日期
        bool uefi{};                       ///< 是否观察到 EFI sysfs；false 不证明 Legacy 启动
        std::string bios_release{};        ///< SMBIOS BIOS revision，与厂商版本字符串区分
        std::string ec_firmware_release{}; ///< 嵌入式控制器固件 revision
    };

    /// @brief 虚拟化
    struct Virtualization
    {
        VirtualizationKind kind{}; ///< 虚拟化类型
        std::string hypervisor;    ///< 虚拟机监控程序
    };

    /// @brief 平台信息
    /// @details 描述系统的基本标识：主机、操作系统、内核、架构、固件、虚拟化。
    struct Platform
    {
        Host host;                                    ///< 主机标识
        Os os;                                        ///< 操作系统
        Kernel kernel;                                ///< 内核
        Architecture architecture;                    ///< 硬件架构
        std::optional<Firmware> firmware;             ///< 固件（可能采集不到）
        std::optional<Virtualization> virtualization; ///< 虚拟化（可能采集不到）
        std::optional<Baseboard> baseboard{};         ///< 主板（可能采集不到）
        std::optional<Chassis> chassis{};             ///< 机箱（可能采集不到）
    };

} // namespace sysal
