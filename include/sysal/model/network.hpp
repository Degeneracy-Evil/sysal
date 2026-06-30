/// @file network.hpp
/// @brief 网络数据模型
/// @details 定义网络子系统的数据结构：NetworkInterface、Network，
///          描述系统的网络接口信息，并提供可见性筛选与按名查找接口。

#pragma once

#include "sysal/types/enums.hpp"
#include "sysal/types/ids.hpp"
#include "sysal/types/units.hpp"
#include "sysal/types/value_types.hpp"

#include <optional>
#include <vector>

namespace sysal
{

/// @brief 单个网络接口
struct NetworkInterface
{
    InterfaceName name;                    ///< 接口名称
    MacAddress mac;                        ///< MAC 地址
    InterfaceState state{};                ///< 链路状态
    std::optional<Bandwidth> speed;        ///< 链路速率（可能未知）
    std::vector<IpAddress> addresses;      ///< 绑定的 IP 地址列表
    std::optional<PciAddress> pci_address; ///< PCI 地址（可能无）
    bool visible_to_current_process{};     ///< 当前进程是否可见
};

/// @brief 网络子系统聚合
/// @details 持有全部网络接口并提供可见性筛选与按名查找接口。
struct Network
{
    std::vector<NetworkInterface> interfaces; ///< 网络接口列表

    /// @brief 获取当前进程可见的接口
    /// @return 指向可见接口的指针向量
    std::vector<const NetworkInterface*> visible() const;

    /// @brief 按接口名查找
    /// @param name 接口名称
    /// @return 指向匹配接口的指针，未找到则返回 nullptr
    const NetworkInterface* find(const InterfaceName& name) const;
};

} // namespace sysal
