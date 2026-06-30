/// @file resource.cpp
/// @brief 资源查询方法实现
/// @details 实现 Cpu、Accelerators、Network、Pci 的查询方法，
///          包括按 ID 查找、按父 ID 过滤、可见性筛选。

#include "sysal/model/accelerator.hpp"
#include "sysal/model/cpu.hpp"
#include "sysal/model/network.hpp"
#include "sysal/model/pci.hpp"

namespace sysal
{

// ---- Cpu ----

const CpuPackage* Cpu::find_package(CpuPackageId id) const
{
    for(const auto& pkg : packages)
    {
        if(pkg.id == id)
        {
            return &pkg;
        }
    }
    return nullptr;
}

const CpuCore* Cpu::find_core(CpuCoreId id) const
{
    for(const auto& core : cores)
    {
        if(core.id == id)
        {
            return &core;
        }
    }
    return nullptr;
}

const LogicalCpu* Cpu::find_logical_cpu(LogicalCpuId id) const
{
    for(const auto& cpu : logical_cpus)
    {
        if(cpu.id == id)
        {
            return &cpu;
        }
    }
    return nullptr;
}

std::vector<const LogicalCpu*> Cpu::logical_cpus_of_package(CpuPackageId id) const
{
    std::vector<const LogicalCpu*> result;
    for(const auto& cpu : logical_cpus)
    {
        if(cpu.package_id == id)
        {
            result.push_back(&cpu);
        }
    }
    return result;
}

std::vector<const LogicalCpu*> Cpu::logical_cpus_of_core(CpuCoreId id) const
{
    std::vector<const LogicalCpu*> result;
    for(const auto& cpu : logical_cpus)
    {
        if(cpu.core_id == id)
        {
            result.push_back(&cpu);
        }
    }
    return result;
}

std::vector<const CpuCore*> Cpu::cores_of_package(CpuPackageId id) const
{
    std::vector<const CpuCore*> result;
    for(const auto& core : cores)
    {
        if(core.package_id == id)
        {
            result.push_back(&core);
        }
    }
    return result;
}

std::vector<const LogicalCpu*> Cpu::visible_logical_cpus() const
{
    std::vector<const LogicalCpu*> result;
    for(const auto& cpu : logical_cpus)
    {
        if(cpu.visible_to_current_process)
        {
            result.push_back(&cpu);
        }
    }
    return result;
}

// ---- Accelerators ----

std::vector<const AcceleratorDevice*> Accelerators::by_kind(AcceleratorKind kind) const
{
    std::vector<const AcceleratorDevice*> result;
    for(const auto& dev : devices)
    {
        if(dev.kind == kind)
        {
            result.push_back(&dev);
        }
    }
    return result;
}

std::vector<const AcceleratorDevice*> Accelerators::gpus() const
{
    return by_kind(AcceleratorKind::Gpu);
}

std::vector<const AcceleratorDevice*> Accelerators::npus() const
{
    return by_kind(AcceleratorKind::Npu);
}

std::vector<const AcceleratorDevice*> Accelerators::fpgas() const
{
    return by_kind(AcceleratorKind::Fpga);
}

std::vector<const AcceleratorDevice*> Accelerators::visible() const
{
    std::vector<const AcceleratorDevice*> result;
    for(const auto& dev : devices)
    {
        if(dev.visible_to_current_process)
        {
            result.push_back(&dev);
        }
    }
    return result;
}

const AcceleratorDevice* Accelerators::find(AcceleratorId id) const
{
    for(const auto& dev : devices)
    {
        if(dev.id == id)
        {
            return &dev;
        }
    }
    return nullptr;
}

// ---- Network ----

std::vector<const NetworkInterface*> Network::visible() const
{
    std::vector<const NetworkInterface*> result;
    for(const auto& iface : interfaces)
    {
        if(iface.visible_to_current_process)
        {
            result.push_back(&iface);
        }
    }
    return result;
}

const NetworkInterface* Network::find(const InterfaceName& name) const
{
    for(const auto& iface : interfaces)
    {
        if(iface.name == name)
        {
            return &iface;
        }
    }
    return nullptr;
}

// ---- Pci ----

const PciDevice* Pci::find(PciAddress addr) const
{
    for(const auto& dev : devices)
    {
        if(dev.address == addr)
        {
            return &dev;
        }
    }
    return nullptr;
}

} // namespace sysal
