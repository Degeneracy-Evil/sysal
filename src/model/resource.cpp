/// @file resource.cpp
/// @brief 资源查询方法实现
/// @details 实现 Cpu、Accelerators、Network、Pci 的查询方法，
///          包括按 ID 查找、按父 ID 过滤、可见性筛选。

#include "sysal/model/accelerator.hpp"
#include "sysal/model/cpu.hpp"
#include "sysal/model/network.hpp"
#include "sysal/model/pci.hpp"

#include <type_traits>

namespace
{

    template <typename Range, typename Member, typename Key>
    [[nodiscard]] auto *find_by_member(const Range &range, Member member, const Key &key)
    {
        for(const auto &item : range)
        {
            if(item.*member == key)
            {
                return &item;
            }
        }
        return static_cast<const std::remove_reference_t<decltype(range.front())> *>(nullptr);
    }

    template <typename Range, typename Pred> [[nodiscard]] auto filter_by(const Range &range, Pred pred)
    {
        std::vector<const std::remove_reference_t<decltype(range.front())> *> result;
        for(const auto &item : range)
        {
            if(pred(item))
            {
                result.push_back(&item);
            }
        }
        return result;
    }

} // namespace

namespace sysal
{

    // ---- Cpu ----

    const CpuPackage *Cpu::find_package(CpuPackageId id) const
    {
        return find_by_member(packages, &CpuPackage::id, id);
    }

    const CpuCore *Cpu::find_core(CpuCoreId id) const
    {
        return find_by_member(cores, &CpuCore::id, id);
    }

    const LogicalCpu *Cpu::find_logical_cpu(LogicalCpuId id) const
    {
        return find_by_member(logical_cpus, &LogicalCpu::id, id);
    }

    std::vector<const LogicalCpu *> Cpu::logical_cpus_of_package(CpuPackageId id) const
    {
        return filter_by(logical_cpus, [id](const LogicalCpu &cpu) { return cpu.package_id == id; });
    }

    std::vector<const LogicalCpu *> Cpu::logical_cpus_of_core(CpuCoreId id) const
    {
        return filter_by(logical_cpus, [id](const LogicalCpu &cpu) { return cpu.core_id == id; });
    }

    std::vector<const CpuCore *> Cpu::cores_of_package(CpuPackageId id) const
    {
        return filter_by(cores, [id](const CpuCore &core) { return core.package_id == id; });
    }

    std::vector<const LogicalCpu *> Cpu::visible_logical_cpus() const
    {
        return filter_by(logical_cpus, [](const LogicalCpu &cpu) { return cpu.visible_to_current_process; });
    }

    // ---- Accelerators ----

    std::vector<const AcceleratorDevice *> Accelerators::by_kind(AcceleratorKind kind) const
    {
        std::vector<const AcceleratorDevice *> result;
        for(const auto &dev : devices)
        {
            if(dev.kind == kind)
            {
                result.push_back(&dev);
            }
        }
        return result;
    }

    std::vector<const AcceleratorDevice *> Accelerators::gpus() const
    {
        return by_kind(AcceleratorKind::Gpu);
    }

    std::vector<const AcceleratorDevice *> Accelerators::npus() const
    {
        return by_kind(AcceleratorKind::Npu);
    }

    std::vector<const AcceleratorDevice *> Accelerators::fpgas() const
    {
        return by_kind(AcceleratorKind::Fpga);
    }

    std::vector<const AcceleratorDevice *> Accelerators::visible() const
    {
        return filter_by(devices, [](const AcceleratorDevice &dev) { return dev.visible_to_current_process; });
    }

    const AcceleratorDevice *Accelerators::find(AcceleratorId id) const
    {
        return find_by_member(devices, &AcceleratorDevice::id, id);
    }

    // ---- Network ----

    std::vector<const NetworkInterface *> Network::visible() const
    {
        return filter_by(interfaces, [](const NetworkInterface &iface) { return iface.visible_to_current_process; });
    }

    const NetworkInterface *Network::find(const InterfaceName &name) const
    {
        return find_by_member(interfaces, &NetworkInterface::name, name);
    }

    // ---- Pci ----

    const PciDevice *Pci::find(PciAddress addr) const
    {
        return find_by_member(devices, &PciDevice::address, addr);
    }

} // namespace sysal
