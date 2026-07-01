#include "sysal/model/accelerator.hpp"
#include "sysal/model/cpu.hpp"
#include "sysal/model/pci.hpp"
#include "sysal/model/raw_store.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>

int main()
{
    // ---- Cpu 查询方法 ----

    sysal::Cpu cpu;
    cpu.arch = sysal::Arch::X86_64;

    // 2 packages
    cpu.packages.push_back(sysal::CpuPackage{
        sysal::CpuPackageId{0}, sysal::Vendor{"Intel"}, sysal::DeviceName{"Xeon Gold"}, 4, 8,
        sysal::Frequency{2400000000}, sysal::Frequency{3800000000}});
    cpu.packages.push_back(sysal::CpuPackage{
        sysal::CpuPackageId{1}, sysal::Vendor{"Intel"}, sysal::DeviceName{"Xeon Gold"}, 4, 8,
        sysal::Frequency{2400000000}, sysal::Frequency{3800000000}});

    // 4 cores (2 per package)
    cpu.cores.push_back(
        sysal::CpuCore{sysal::CpuCoreId{0}, sysal::CpuPackageId{0}, 2, sysal::NumaNodeId{0}});
    cpu.cores.push_back(
        sysal::CpuCore{sysal::CpuCoreId{1}, sysal::CpuPackageId{0}, 2, sysal::NumaNodeId{0}});
    cpu.cores.push_back(
        sysal::CpuCore{sysal::CpuCoreId{2}, sysal::CpuPackageId{1}, 2, sysal::NumaNodeId{1}});
    cpu.cores.push_back(
        sysal::CpuCore{sysal::CpuCoreId{3}, sysal::CpuPackageId{1}, 2, sysal::NumaNodeId{1}});

    // 8 logical CPUs (2 per core)
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{0}, sysal::CpuCoreId{0},
                                                 sysal::CpuPackageId{0}, sysal::NumaNodeId{0},
                                                 true});
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{1}, sysal::CpuCoreId{0},
                                                 sysal::CpuPackageId{0}, sysal::NumaNodeId{0},
                                                 true});
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{2}, sysal::CpuCoreId{1},
                                                 sysal::CpuPackageId{0}, sysal::NumaNodeId{0},
                                                 false});
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{3}, sysal::CpuCoreId{1},
                                                 sysal::CpuPackageId{0}, sysal::NumaNodeId{0},
                                                 false});
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{4}, sysal::CpuCoreId{2},
                                                 sysal::CpuPackageId{1}, sysal::NumaNodeId{1},
                                                 true});
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{5}, sysal::CpuCoreId{2},
                                                 sysal::CpuPackageId{1}, sysal::NumaNodeId{1},
                                                 true});
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{6}, sysal::CpuCoreId{3},
                                                 sysal::CpuPackageId{1}, sysal::NumaNodeId{1},
                                                 false});
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{7}, sysal::CpuCoreId{3},
                                                 sysal::CpuPackageId{1}, sysal::NumaNodeId{1},
                                                 false});

    // find_package
    assert(cpu.find_package(sysal::CpuPackageId{0}) != nullptr);
    assert(cpu.find_package(sysal::CpuPackageId{0})->id == sysal::CpuPackageId{0});
    assert(cpu.find_package(sysal::CpuPackageId{1}) != nullptr);
    assert(cpu.find_package(sysal::CpuPackageId{99}) == nullptr);

    // find_core
    assert(cpu.find_core(sysal::CpuCoreId{0}) != nullptr);
    assert(cpu.find_core(sysal::CpuCoreId{99}) == nullptr);

    // find_logical_cpu
    assert(cpu.find_logical_cpu(sysal::LogicalCpuId{0}) != nullptr);
    assert(cpu.find_logical_cpu(sysal::LogicalCpuId{99}) == nullptr);

    // logical_cpus_of_package
    auto pkg0_cpus = cpu.logical_cpus_of_package(sysal::CpuPackageId{0});
    assert(pkg0_cpus.size() == 4);
    auto pkg1_cpus = cpu.logical_cpus_of_package(sysal::CpuPackageId{1});
    assert(pkg1_cpus.size() == 4);

    // logical_cpus_of_core
    auto core0_cpus = cpu.logical_cpus_of_core(sysal::CpuCoreId{0});
    assert(core0_cpus.size() == 2);

    // cores_of_package
    auto pkg0_cores = cpu.cores_of_package(sysal::CpuPackageId{0});
    assert(pkg0_cores.size() == 2);

    // visible_logical_cpus
    auto visible = cpu.visible_logical_cpus();
    assert(visible.size() == 4);
    for(const auto* lc : visible)
    {
        assert(lc->visible_to_current_process);
    }

    // ---- Accelerators 查询方法 ----

    sysal::Accelerators acc;
    acc.devices.push_back(sysal::AcceleratorDevice{
        sysal::AcceleratorId{0}, sysal::AcceleratorKind::Gpu, sysal::Vendor{"NVIDIA"},
        sysal::DeviceName{"A100"}, sysal::PciAddress{0, 1, 0, 0}, sysal::NumaNodeId{0},
        sysal::MemorySize{85899345920}, std::nullopt, true});
    acc.devices.push_back(sysal::AcceleratorDevice{
        sysal::AcceleratorId{1}, sysal::AcceleratorKind::Gpu, sysal::Vendor{"NVIDIA"},
        sysal::DeviceName{"A100"}, sysal::PciAddress{0, 2, 0, 0}, sysal::NumaNodeId{1},
        sysal::MemorySize{85899345920}, std::nullopt, false});
    acc.devices.push_back(
        sysal::AcceleratorDevice{sysal::AcceleratorId{2}, sysal::AcceleratorKind::Npu,
                                 sysal::Vendor{"Huawei"}, sysal::DeviceName{"Ascend 910"},
                                 std::nullopt, std::nullopt, std::nullopt, std::nullopt, true});

    // gpus
    auto gpus = acc.gpus();
    assert(gpus.size() == 2);

    // npus
    auto npus = acc.npus();
    assert(npus.size() == 1);

    // by_kind
    auto by_gpu = acc.by_kind(sysal::AcceleratorKind::Gpu);
    assert(by_gpu.size() == 2);
    auto by_fpga = acc.by_kind(sysal::AcceleratorKind::Fpga);
    assert(by_fpga.empty());

    // visible
    auto acc_visible = acc.visible();
    assert(acc_visible.size() == 2);

    // find
    assert(acc.find(sysal::AcceleratorId{0}) != nullptr);
    assert(acc.find(sysal::AcceleratorId{99}) == nullptr);

    // ---- Pci 查询方法 ----

    sysal::Pci pci;
    pci.devices.push_back(sysal::PciDevice{sysal::PciAddress{0, 1, 0, 0}, sysal::Vendor{"NVIDIA"},
                                           sysal::DeviceName{"A100"}, sysal::PciClass{"030000"},
                                           sysal::NumaNodeId{0}});
    pci.devices.push_back(sysal::PciDevice{sysal::PciAddress{0, 2, 0, 0}, sysal::Vendor{"NVIDIA"},
                                           sysal::DeviceName{"A100"}, sysal::PciClass{"030000"},
                                           sysal::NumaNodeId{1}});

    assert(pci.find(sysal::PciAddress{0, 1, 0, 0}) != nullptr);
    assert(pci.find(sysal::PciAddress{0, 1, 0, 0})->vendor.value == "NVIDIA");
    assert(pci.find(sysal::PciAddress{0, 99, 0, 0}) == nullptr);

    // ---- RawStore 查询方法 ----

    sysal::RawStore store;
    auto now = std::chrono::system_clock::now();
    store.records.push_back(sysal::RawRecord{sysal::RawSource::ProcCpuInfo, "/proc/cpuinfo",
                                             "payload1", sysal::CollectStatus::Success, now});
    store.records.push_back(sysal::RawRecord{sysal::RawSource::ProcMemInfo, "/proc/meminfo",
                                             "payload2", sysal::CollectStatus::Success, now});
    store.records.push_back(sysal::RawRecord{sysal::RawSource::ProcCpuInfo, "/proc/cpuinfo2",
                                             "payload3", sysal::CollectStatus::Partial, now});

    // get_all
    auto cpuinfo_all = store.get_all(sysal::RawSource::ProcCpuInfo);
    assert(cpuinfo_all.size() == 2);
    auto meminfo_all = store.get_all(sysal::RawSource::ProcMemInfo);
    assert(meminfo_all.size() == 1);

    // get
    auto cpuinfo_specific = store.get(sysal::RawSource::ProcCpuInfo, "/proc/cpuinfo");
    assert(cpuinfo_specific.size() == 1);
    assert(cpuinfo_specific[0]->payload == "payload1");

    // has
    assert(store.has(sysal::RawSource::ProcCpuInfo));
    assert(store.has(sysal::RawSource::ProcMemInfo));
    assert(!store.has(sysal::RawSource::SysfsCpu));

    // count
    assert(store.count(sysal::RawSource::ProcCpuInfo) == 2);
    assert(store.count(sysal::RawSource::ProcMemInfo) == 1);
    assert(store.count(sysal::RawSource::SysfsCpu) == 0);

    return 0;
}
