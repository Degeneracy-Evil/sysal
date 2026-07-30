#include "sysal/model/accelerator.hpp"
#include "sysal/model/cpu.hpp"
#include "sysal/model/pci.hpp"
#include "sysal/model/raw_store.hpp"

#include "test_macros.hpp"
#include <chrono>
#include <cstdint>

int main()
{
    // ---- Cpu 查询方法 ----

    sysal::Cpu cpu;
    cpu.arch = sysal::Arch::X86_64;

    // 2 packages
    cpu.packages.push_back(sysal::CpuPackage{sysal::CpuPackageId{0}, sysal::Vendor{"Intel"},
                                             sysal::DeviceName{"Xeon Gold"}, 4, 8, sysal::Frequency{2400000000},
                                             sysal::Frequency{3800000000}});
    cpu.packages.push_back(sysal::CpuPackage{sysal::CpuPackageId{1}, sysal::Vendor{"Intel"},
                                             sysal::DeviceName{"Xeon Gold"}, 4, 8, sysal::Frequency{2400000000},
                                             sysal::Frequency{3800000000}});

    // 4 cores (2 per package)
    cpu.cores.push_back(sysal::CpuCore{sysal::CpuCoreId{0}, sysal::CpuPackageId{0}, 2, sysal::NumaNodeId{0}});
    cpu.cores.push_back(sysal::CpuCore{sysal::CpuCoreId{1}, sysal::CpuPackageId{0}, 2, sysal::NumaNodeId{0}});
    cpu.cores.push_back(sysal::CpuCore{sysal::CpuCoreId{2}, sysal::CpuPackageId{1}, 2, sysal::NumaNodeId{1}});
    cpu.cores.push_back(sysal::CpuCore{sysal::CpuCoreId{3}, sysal::CpuPackageId{1}, 2, sysal::NumaNodeId{1}});

    // 8 logical CPUs (2 per core)
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{0}, sysal::CpuCoreId{0}, sysal::CpuPackageId{0},
                                                 sysal::NumaNodeId{0}, true});
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{1}, sysal::CpuCoreId{0}, sysal::CpuPackageId{0},
                                                 sysal::NumaNodeId{0}, true});
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{2}, sysal::CpuCoreId{1}, sysal::CpuPackageId{0},
                                                 sysal::NumaNodeId{0}, false});
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{3}, sysal::CpuCoreId{1}, sysal::CpuPackageId{0},
                                                 sysal::NumaNodeId{0}, false});
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{4}, sysal::CpuCoreId{2}, sysal::CpuPackageId{1},
                                                 sysal::NumaNodeId{1}, true});
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{5}, sysal::CpuCoreId{2}, sysal::CpuPackageId{1},
                                                 sysal::NumaNodeId{1}, true});
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{6}, sysal::CpuCoreId{3}, sysal::CpuPackageId{1},
                                                 sysal::NumaNodeId{1}, false});
    cpu.logical_cpus.push_back(sysal::LogicalCpu{sysal::LogicalCpuId{7}, sysal::CpuCoreId{3}, sysal::CpuPackageId{1},
                                                 sysal::NumaNodeId{1}, false});

    // find_package
    CHECK(cpu.find_package(sysal::CpuPackageId{0}) != nullptr);
    CHECK(cpu.find_package(sysal::CpuPackageId{0})->id == sysal::CpuPackageId{0});
    CHECK(cpu.find_package(sysal::CpuPackageId{1}) != nullptr);
    CHECK(cpu.find_package(sysal::CpuPackageId{99}) == nullptr);

    // find_core
    CHECK(cpu.find_core(sysal::CpuCoreId{0}) != nullptr);
    CHECK(cpu.find_core(sysal::CpuCoreId{99}) == nullptr);

    // find_logical_cpu
    CHECK(cpu.find_logical_cpu(sysal::LogicalCpuId{0}) != nullptr);
    CHECK(cpu.find_logical_cpu(sysal::LogicalCpuId{99}) == nullptr);

    // logical_cpus_of_package
    auto pkg0_cpus = cpu.logical_cpus_of_package(sysal::CpuPackageId{0});
    CHECK(pkg0_cpus.size() == 4);
    auto pkg1_cpus = cpu.logical_cpus_of_package(sysal::CpuPackageId{1});
    CHECK(pkg1_cpus.size() == 4);

    // logical_cpus_of_core
    auto core0_cpus = cpu.logical_cpus_of_core(sysal::CpuCoreId{0});
    CHECK(core0_cpus.size() == 2);

    // cores_of_package
    auto pkg0_cores = cpu.cores_of_package(sysal::CpuPackageId{0});
    CHECK(pkg0_cores.size() == 2);

    // visible_logical_cpus
    auto visible = cpu.visible_logical_cpus();
    CHECK(visible.size() == 4);
    for(const auto *lc : visible)
    {
        CHECK(lc->visible_to_current_process);
    }

    // ---- Accelerators 查询方法 ----

    sysal::Accelerators acc;
    acc.devices.push_back(sysal::AcceleratorDevice{
        sysal::AcceleratorId{0}, sysal::AcceleratorKind::Gpu, sysal::Vendor{"NVIDIA"}, sysal::DeviceName{"A100"},
        sysal::PciAddress{0, 1, 0, 0}, sysal::NumaNodeId{0}, sysal::MemorySize{85899345920}, std::nullopt, true});
    acc.devices.push_back(sysal::AcceleratorDevice{
        sysal::AcceleratorId{1}, sysal::AcceleratorKind::Gpu, sysal::Vendor{"NVIDIA"}, sysal::DeviceName{"A100"},
        sysal::PciAddress{0, 2, 0, 0}, sysal::NumaNodeId{1}, sysal::MemorySize{85899345920}, std::nullopt, false});
    acc.devices.push_back(sysal::AcceleratorDevice{sysal::AcceleratorId{2}, sysal::AcceleratorKind::Npu,
                                                   sysal::Vendor{"Huawei"}, sysal::DeviceName{"Ascend 910"},
                                                   std::nullopt, std::nullopt, std::nullopt, std::nullopt, true});

    // gpus
    auto gpus = acc.gpus();
    CHECK(gpus.size() == 2);

    // npus
    auto npus = acc.npus();
    CHECK(npus.size() == 1);

    // by_kind
    auto by_gpu = acc.by_kind(sysal::AcceleratorKind::Gpu);
    CHECK(by_gpu.size() == 2);
    auto by_fpga = acc.by_kind(sysal::AcceleratorKind::Fpga);
    CHECK(by_fpga.empty());

    // visible
    auto acc_visible = acc.visible();
    CHECK(acc_visible.size() == 2);

    // find
    CHECK(acc.find(sysal::AcceleratorId{0}) != nullptr);
    CHECK(acc.find(sysal::AcceleratorId{99}) == nullptr);

    // ---- Pci 查询方法 ----

    sysal::Pci pci;
    pci.devices.push_back(sysal::PciDevice{sysal::PciAddress{0, 1, 0, 0}, sysal::Vendor{"NVIDIA"},
                                           sysal::DeviceName{"A100"}, sysal::PciClass{"030000"}, sysal::NumaNodeId{0}});
    pci.devices.push_back(sysal::PciDevice{sysal::PciAddress{0, 2, 0, 0}, sysal::Vendor{"NVIDIA"},
                                           sysal::DeviceName{"A100"}, sysal::PciClass{"030000"}, sysal::NumaNodeId{1}});

    CHECK(pci.find(sysal::PciAddress{0, 1, 0, 0}) != nullptr);
    CHECK(pci.find(sysal::PciAddress{0, 1, 0, 0})->vendor.value == "NVIDIA");
    CHECK(pci.find(sysal::PciAddress{0, 99, 0, 0}) == nullptr);

    // ---- RawStore 查询方法 ----

    sysal::RawStore store;
    auto now = std::chrono::system_clock::now();
    store.records.push_back(sysal::RawRecord{sysal::RawSource::ProcCpuInfo, "/proc/cpuinfo", "payload1",
                                             sysal::CollectStatus::Success, now});
    store.records.push_back(sysal::RawRecord{sysal::RawSource::ProcMemInfo, "/proc/meminfo", "payload2",
                                             sysal::CollectStatus::Success, now});
    store.records.push_back(sysal::RawRecord{sysal::RawSource::ProcCpuInfo, "/proc/cpuinfo2", "payload3",
                                             sysal::CollectStatus::Partial, now});

    // get_all
    auto cpuinfo_all = store.get_all(sysal::RawSource::ProcCpuInfo);
    CHECK(cpuinfo_all.size() == 2);
    auto meminfo_all = store.get_all(sysal::RawSource::ProcMemInfo);
    CHECK(meminfo_all.size() == 1);

    // get
    auto cpuinfo_specific = store.get(sysal::RawSource::ProcCpuInfo, "/proc/cpuinfo");
    CHECK(cpuinfo_specific.size() == 1);
    CHECK(cpuinfo_specific[0]->payload == "payload1");

    // has
    CHECK(store.has(sysal::RawSource::ProcCpuInfo));
    CHECK(store.has(sysal::RawSource::ProcMemInfo));
    CHECK(!store.has(sysal::RawSource::SysfsCpu));

    // count
    CHECK(store.count(sysal::RawSource::ProcCpuInfo) == 2);
    CHECK(store.count(sysal::RawSource::ProcMemInfo) == 1);
    CHECK(store.count(sysal::RawSource::SysfsCpu) == 0);

    TEST_SUMMARY();
}
