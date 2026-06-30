/// @file resource_info.cpp
/// @brief ResourceInfo 子系统的查询辅助函数实现
/// @details 为 CPU、加速器、网络、PCI 子系统提供基于 ID 的查找、
///          按属性过滤以及当前进程可见性筛选等便捷方法。

#include "sysal/resource_info.hpp"
#include "detail/algorithm.hpp"

namespace sysal
{

using detail::filter_if;
using detail::find_if;

/// @brief 按包 ID 查找 CPU 物理包
/// @param id 目标 CPU 包 ID
/// @return 找到返回指向该包的指针，未找到返回 nullptr
const CpuPackage* CpuSubsystem::find_package(CpuPackageId id) const
{
    return find_if(packages, [id](const auto& pkg) { return pkg.id == id; });
}

/// @brief 按核 ID 查找 CPU 物理核
/// @param id 目标 CPU 核 ID
/// @return 找到返回指向该核的指针，未找到返回 nullptr
const CpuCore* CpuSubsystem::find_core(CpuCoreId id) const
{
    return find_if(cores, [id](const auto& core) { return core.id == id; });
}

/// @brief 按逻辑 CPU ID 查找逻辑 CPU
/// @param id 目标逻辑 CPU ID
/// @return 找到返回指向该逻辑 CPU 的指针，未找到返回 nullptr
const LogicalCpu* CpuSubsystem::find_logical_cpu(LogicalCpuId id) const
{
    return find_if(logical_cpus, [id](const auto& cpu) { return cpu.id == id; });
}

/// @brief 获取指定物理包下的全部逻辑 CPU
/// @param id 目标 CPU 包 ID
/// @return 属于该包的逻辑 CPU 指针列表
std::vector<const LogicalCpu*> CpuSubsystem::logical_cpus_of_package(CpuPackageId id) const
{
    return filter_if(logical_cpus, [id](const auto& cpu) { return cpu.package_id == id; });
}

/// @brief 获取指定物理核下的全部逻辑 CPU
/// @param id 目标 CPU 核 ID
/// @return 属于该核的逻辑 CPU 指针列表
std::vector<const LogicalCpu*> CpuSubsystem::logical_cpus_of_core(CpuCoreId id) const
{
    return filter_if(logical_cpus, [id](const auto& cpu) { return cpu.core_id == id; });
}

/// @brief 获取指定物理包下的全部物理核
/// @param id 目标 CPU 包 ID
/// @return 属于该包的物理核指针列表
std::vector<const CpuCore*> CpuSubsystem::cores_of_package(CpuPackageId id) const
{
    return filter_if(cores, [id](const auto& core) { return core.package_id == id; });
}

/// @brief 获取当前进程可见的全部逻辑 CPU
/// @return 可见逻辑 CPU 指针列表
std::vector<const LogicalCpu*> CpuSubsystem::visible_logical_cpus() const
{
    return filter_if(logical_cpus, [](const auto& cpu) { return cpu.visible_to_current_process; });
}

/// @brief 按加速器类型筛选设备
/// @param kind 目标加速器类型
/// @return 匹配类型的加速器设备指针列表
std::vector<const AcceleratorDevice*> AcceleratorSubsystem::by_kind(AcceleratorKind kind) const
{
    return filter_if(devices, [kind](const auto& dev) { return dev.kind == kind; });
}

/// @brief 获取全部 GPU 设备
/// @return 类型为 Gpu 的加速器设备指针列表
std::vector<const AcceleratorDevice*> AcceleratorSubsystem::gpus() const
{
    return by_kind(AcceleratorKind::Gpu);
}

/// @brief 获取全部 NPU 设备
/// @return 类型为 Npu 的加速器设备指针列表
std::vector<const AcceleratorDevice*> AcceleratorSubsystem::npus() const
{
    return by_kind(AcceleratorKind::Npu);
}

/// @brief 获取全部 FPGA 设备
/// @return 类型为 Fpga 的加速器设备指针列表
std::vector<const AcceleratorDevice*> AcceleratorSubsystem::fpgas() const
{
    return by_kind(AcceleratorKind::Fpga);
}

/// @brief 获取当前进程可见的全部加速器设备
/// @return 可见加速器设备指针列表
std::vector<const AcceleratorDevice*> AcceleratorSubsystem::visible() const
{
    return filter_if(devices, [](const auto& dev) { return dev.visible_to_current_process; });
}

/// @brief 按加速器 ID 查找设备
/// @param id 目标加速器 ID
/// @return 找到返回指向该设备的指针，未找到返回 nullptr
const AcceleratorDevice* AcceleratorSubsystem::find(AcceleratorId id) const
{
    return find_if(devices, [id](const auto& dev) { return dev.id == id; });
}

/// @brief 获取当前进程可见的全部网络接口
/// @return 可见网络接口指针列表
std::vector<const NetworkInterface*> NetworkSubsystem::visible() const
{
    return filter_if(interfaces,
                     [](const auto& iface) { return iface.visible_to_current_process; });
}

/// @brief 按接口名查找网络接口
/// @param name 目标接口名
/// @return 找到返回指向该接口的指针，未找到返回 nullptr
const NetworkInterface* NetworkSubsystem::find(const InterfaceName& name) const
{
    return find_if(interfaces, [&name](const auto& iface) { return iface.name == name; });
}

/// @brief 按 PCI 地址查找 PCI 设备
/// @param addr 目标 PCI 地址
/// @return 找到返回指向该设备的指针，未找到返回 nullptr
const PciDevice* PciSubsystem::find(PciAddress addr) const
{
    return find_if(devices, [addr](const auto& dev) { return dev.address == addr; });
}

} // namespace sysal
