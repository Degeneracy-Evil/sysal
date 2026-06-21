#include "sysal/resource_info.hpp"
#include "detail/algorithm.hpp"

namespace sysal
{

using detail::filter_if;
using detail::find_if;

const CpuPackage* CpuSubsystem::find_package(CpuPackageId id) const
{
    return find_if(packages, [id](const auto& pkg) { return pkg.id == id; });
}

const CpuCore* CpuSubsystem::find_core(CpuCoreId id) const
{
    return find_if(cores, [id](const auto& core) { return core.id == id; });
}

const LogicalCpu* CpuSubsystem::find_logical_cpu(LogicalCpuId id) const
{
    return find_if(logical_cpus, [id](const auto& cpu) { return cpu.id == id; });
}

std::vector<const LogicalCpu*> CpuSubsystem::logical_cpus_of_package(CpuPackageId id) const
{
    return filter_if(logical_cpus, [id](const auto& cpu) { return cpu.package_id == id; });
}

std::vector<const LogicalCpu*> CpuSubsystem::logical_cpus_of_core(CpuCoreId id) const
{
    return filter_if(logical_cpus, [id](const auto& cpu) { return cpu.core_id == id; });
}

std::vector<const CpuCore*> CpuSubsystem::cores_of_package(CpuPackageId id) const
{
    return filter_if(cores, [id](const auto& core) { return core.package_id == id; });
}

std::vector<const LogicalCpu*> CpuSubsystem::visible_logical_cpus() const
{
    return filter_if(logical_cpus, [](const auto& cpu) { return cpu.visible_to_current_process; });
}

std::vector<const AcceleratorDevice*> AcceleratorSubsystem::by_kind(AcceleratorKind kind) const
{
    return filter_if(devices, [kind](const auto& dev) { return dev.kind == kind; });
}

std::vector<const AcceleratorDevice*> AcceleratorSubsystem::gpus() const
{
    return by_kind(AcceleratorKind::Gpu);
}

std::vector<const AcceleratorDevice*> AcceleratorSubsystem::npus() const
{
    return by_kind(AcceleratorKind::Npu);
}

std::vector<const AcceleratorDevice*> AcceleratorSubsystem::fpgas() const
{
    return by_kind(AcceleratorKind::Fpga);
}

std::vector<const AcceleratorDevice*> AcceleratorSubsystem::visible() const
{
    return filter_if(devices, [](const auto& dev) { return dev.visible_to_current_process; });
}

const AcceleratorDevice* AcceleratorSubsystem::find(AcceleratorId id) const
{
    return find_if(devices, [id](const auto& dev) { return dev.id == id; });
}

std::vector<const NetworkInterface*> NetworkSubsystem::visible() const
{
    return filter_if(interfaces,
                     [](const auto& iface) { return iface.visible_to_current_process; });
}

const NetworkInterface* NetworkSubsystem::find(const InterfaceName& name) const
{
    return find_if(interfaces, [&name](const auto& iface) { return iface.name == name; });
}

const PciDevice* PciSubsystem::find(PciAddress addr) const
{
    return find_if(devices, [addr](const auto& dev) { return dev.address == addr; });
}

} // namespace sysal
