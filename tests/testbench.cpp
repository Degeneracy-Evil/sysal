/// @file testbench.cpp
/// @brief sysal 全量能力测试
/// @details 测试 sysal 库的所有公共 API：完整采集、按域采集、模型查询、
///          JSON 序列化往返、原始证据、刷新、可见性筛选和错误处理。

#include "sysal/core/sysal.hpp"
#include "sysal/serialization/serialization.hpp"
#include "sysal/test/replay.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{

// ── 格式化辅助 ──────────────────────────────────────────────────────────

/// @brief 格式化内存大小为人类可读字符串
/// @param bytes 字节数
/// @return 格式化字符串（如 "4096 MiB"）
std::string format_memory(std::uint64_t bytes)
{
    const auto gib = 1024ULL * 1024 * 1024;
    const auto mib = 1024ULL * 1024;
    const auto kib = 1024ULL;
    if(bytes >= gib)
    {
        return std::to_string(bytes / gib) + " GiB";
    }
    if(bytes >= mib)
    {
        return std::to_string(bytes / mib) + " MiB";
    }
    if(bytes >= kib)
    {
        return std::to_string(bytes / kib) + " KiB";
    }
    return std::to_string(bytes) + " B";
}

/// @brief 格式化 PCI 地址为十六进制零填充
/// @param addr PCI 地址
/// @return 格式化字符串（如 "0000:65:00.0"）
std::string format_pci(const sysal::PciAddress& addr)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04x:%02x:%02x.%x", addr.domain, addr.bus, addr.device,
                  addr.function);
    return buf;
}

// ── 枚举转字符串 ───────────────────────────────────────────────────────

const char* arch_str(sysal::Arch a)
{
    switch(a)
    {
    case sysal::Arch::X86_64:
        return "x86_64";
    case sysal::Arch::AArch64:
        return "AArch64";
    case sysal::Arch::Riscv64:
        return "Riscv64";
    case sysal::Arch::Other:
        return "Other";
    }
    return "?";
}

const char* isa_str(sysal::IsaExtension e)
{
    switch(e)
    {
    case sysal::IsaExtension::Sse42:
        return "SSE4.2";
    case sysal::IsaExtension::Avx:
        return "AVX";
    case sysal::IsaExtension::Avx2:
        return "AVX2";
    case sysal::IsaExtension::Avx512f:
        return "AVX-512F";
    case sysal::IsaExtension::Avx512cd:
        return "AVX-512CD";
    case sysal::IsaExtension::Avx512bw:
        return "AVX-512BW";
    case sysal::IsaExtension::Avx512dq:
        return "AVX-512DQ";
    case sysal::IsaExtension::Avx512vl:
        return "AVX-512VL";
    }
    return "?";
}

const char* state_str(sysal::InterfaceState s)
{
    switch(s)
    {
    case sysal::InterfaceState::Up:
        return "UP";
    case sysal::InterfaceState::Down:
        return "DOWN";
    case sysal::InterfaceState::Unknown:
        return "UNKNOWN";
    }
    return "?";
}

const char* storage_kind_str(sysal::StorageKind k)
{
    switch(k)
    {
    case sysal::StorageKind::Nvme:
        return "NVMe";
    case sysal::StorageKind::Sata:
        return "SATA";
    case sysal::StorageKind::Sas:
        return "SAS";
    case sysal::StorageKind::Other:
        return "Other";
    }
    return "?";
}

const char* accel_kind_str(sysal::AcceleratorKind k)
{
    switch(k)
    {
    case sysal::AcceleratorKind::Gpu:
        return "GPU";
    case sysal::AcceleratorKind::Npu:
        return "NPU";
    case sysal::AcceleratorKind::Fpga:
        return "FPGA";
    case sysal::AcceleratorKind::Other:
        return "Other";
    }
    return "?";
}

const char* virt_kind_str(sysal::VirtualizationKind k)
{
    switch(k)
    {
    case sysal::VirtualizationKind::None:
        return "None";
    case sysal::VirtualizationKind::Kvm:
        return "KVM";
    case sysal::VirtualizationKind::Xen:
        return "Xen";
    case sysal::VirtualizationKind::Vmware:
        return "VMware";
    case sysal::VirtualizationKind::Other:
        return "Other";
    }
    return "?";
}

const char* cgroup_ver_str(sysal::CgroupVersion v)
{
    switch(v)
    {
    case sysal::CgroupVersion::V1:
        return "v1";
    case sysal::CgroupVersion::V2:
        return "v2";
    }
    return "?";
}

const char* container_kind_str(sysal::ContainerKind k)
{
    switch(k)
    {
    case sysal::ContainerKind::Docker:
        return "Docker";
    case sysal::ContainerKind::Podman:
        return "Podman";
    case sysal::ContainerKind::Lxc:
        return "LXC";
    case sysal::ContainerKind::Kubernetes:
        return "Kubernetes";
    case sysal::ContainerKind::Other:
        return "Other";
    }
    return "?";
}

const char* raw_source_str(sysal::RawSource s)
{
    switch(s)
    {
    case sysal::RawSource::ProcCpuInfo:
        return "ProcCpuInfo";
    case sysal::RawSource::ProcMemInfo:
        return "ProcMemInfo";
    case sysal::RawSource::ProcVersion:
        return "ProcVersion";
    case sysal::RawSource::ProcSelfCgroup:
        return "ProcSelfCgroup";
    case sysal::RawSource::ProcSelfStatus:
        return "ProcSelfStatus";
    case sysal::RawSource::ProcOneCgroup:
        return "ProcOneCgroup";
    case sysal::RawSource::SysfsCpu:
        return "SysfsCpu";
    case sysal::RawSource::SysfsNuma:
        return "SysfsNuma";
    case sysal::RawSource::SysfsNet:
        return "SysfsNet";
    case sysal::RawSource::SysfsPci:
        return "SysfsPci";
    case sysal::RawSource::SysfsBlock:
        return "SysfsBlock";
    case sysal::RawSource::SysfsDmi:
        return "SysfsDmi";
    case sysal::RawSource::EtcOsRelease:
        return "EtcOsRelease";
    case sysal::RawSource::RootDockerenv:
        return "RootDockerenv";
    case sysal::RawSource::Uname:
        return "Uname";
    case sysal::RawSource::Lspci:
        return "Lspci";
    case sysal::RawSource::NvidiaSmi:
        return "NvidiaSmi";
    case sysal::RawSource::Nvcc:
        return "Nvcc";
    case sysal::RawSource::Lsblk:
        return "Lsblk";
    case sysal::RawSource::Environment:
        return "Environment";
    case sysal::RawSource::Nvml:
        return "Nvml";
    case sysal::RawSource::Ibverbs:
        return "Ibverbs";
    case sysal::RawSource::HwinfoOutput:
        return "HwinfoOutput";
    }
    return "?";
}

const char* collect_status_str(sysal::CollectStatus s)
{
    switch(s)
    {
    case sysal::CollectStatus::Success:
        return "Success";
    case sysal::CollectStatus::Partial:
        return "Partial";
    case sysal::CollectStatus::Failed:
        return "Failed";
    case sysal::CollectStatus::NotCollected:
        return "NotCollected";
    }
    return "?";
}

// ── 输出辅助 ───────────────────────────────────────────────────────────

void section(const std::string& title) { std::cout << "\n--- " << title << " ---\n"; }

void label(const std::string& key, const std::string& value)
{
    std::cout << "  " << std::left << std::setw(24) << key << ": " << value << "\n";
}

void label(const std::string& key, std::uint64_t value) { label(key, std::to_string(value)); }

void label(const std::string& key, std::uint32_t value) { label(key, std::to_string(value)); }

void label(const std::string& key, std::int32_t value) { label(key, std::to_string(value)); }

void label(const std::string& key, const char* value) { label(key, std::string{value}); }

} // namespace

// ── 主测试 ─────────────────────────────────────────────────────────────

int main()
{
    std::cout << "=== sysal testbench ===\n";

    // ── 1. Full Collection ──────────────────────────────────────────────
    section("1. Full Collection");
    auto sys = sysal::System::collect();

    assert(!sys.info.cpu.logical_cpus.empty());
    assert(!sys.info.cpu.packages.empty());
    assert(sys.info.memory.total_memory.value > 0);
    assert(!sys.info.platform.os.name.empty());
    assert(!sys.meta.succeeded_collectors.empty());
    assert(sys.meta.sysal_version == "0.0.1");
    assert(sys.meta.collect_duration.count() >= 0.0);

    label("Collect duration (s)",
          std::to_string(static_cast<long>(sys.meta.collect_duration.count() * 1000)) + " ms");
    label("Succeeded collectors", sys.meta.succeeded_collectors.size());
    label("Failed collectors", sys.meta.failed_collectors.size());
    std::cout << "  Succeeded:";
    for(const auto& c : sys.meta.succeeded_collectors)
    {
        std::cout << " " << c;
    }
    std::cout << "\n";
    if(!sys.meta.failed_collectors.empty())
    {
        std::cout << "  Failed:";
        for(const auto& c : sys.meta.failed_collectors)
        {
            std::cout << " " << c;
        }
        std::cout << "\n";
    }

    // ── 2. Platform ─────────────────────────────────────────────────────
    section("2. Platform");
    label("Hostname", sys.info.platform.host.hostname);
    label("OS name", sys.info.platform.os.name);
    label("OS version", sys.info.platform.os.version);
    if(!sys.info.platform.os.distribution.empty())
    {
        label("Distribution",
              sys.info.platform.os.distribution + " " + sys.info.platform.os.distribution_version);
    }
    if(!sys.info.platform.os.codename.empty())
    {
        label("Codename", sys.info.platform.os.codename);
    }
    label("Kernel release", sys.info.platform.kernel.release);
    label("Kernel version", sys.info.platform.kernel.version);
    label("Kernel arch", sys.info.platform.kernel.architecture);
    label("Architecture", sys.info.platform.architecture.name);
    label("Bits", std::to_string(sys.info.platform.architecture.bits));
    label("Byte order", sys.info.platform.architecture.byte_order);
    if(sys.info.platform.firmware.has_value())
    {
        label("BIOS vendor", sys.info.platform.firmware->bios_vendor);
        label("BIOS version", sys.info.platform.firmware->bios_version);
        label("BIOS date", sys.info.platform.firmware->bios_date);
        label("UEFI", sys.info.platform.firmware->uefi ? "yes" : "no");
    }
    if(sys.info.platform.virtualization.has_value())
    {
        label("Virt kind", virt_kind_str(sys.info.platform.virtualization->kind));
        label("Hypervisor", sys.info.platform.virtualization->hypervisor);
        label("Container", sys.info.platform.virtualization->container ? "yes" : "no");
    }

    // ── 3. CPU ──────────────────────────────────────────────────────────
    section("3. CPU");
    label("Arch", arch_str(sys.info.cpu.arch));
    label("Packages", sys.info.cpu.packages.size());
    label("Cores", sys.info.cpu.cores.size());
    label("Logical CPUs", sys.info.cpu.logical_cpus.size());
    label("NUMA nodes", sys.info.cpu.numa_nodes.size());

    for(const auto& pkg : sys.info.cpu.packages)
    {
        std::cout << "  Package " << pkg.id << ": " << pkg.model_name.value << "\n";
        label("  Vendor", pkg.vendor.value);
        label("  Physical cores", pkg.physical_cores);
        label("  Logical threads", pkg.logical_threads);
        if(pkg.base_frequency.has_value())
        {
            label("  Base freq (MHz)", pkg.base_frequency->value / 1000);
        }
        if(pkg.max_frequency.has_value())
        {
            label("  Max freq (MHz)", pkg.max_frequency->value / 1000);
        }
    }

    if(!sys.info.cpu.isa_extensions.empty())
    {
        std::cout << "  ISA extensions:";
        for(const auto& ext : sys.info.cpu.isa_extensions)
        {
            std::cout << " " << isa_str(ext);
        }
        std::cout << "\n";
    }

    for(const auto& node : sys.info.cpu.numa_nodes)
    {
        std::cout << "  NUMA node " << node.id << ": CPUs";
        for(const auto& cpu_id : node.cpus)
        {
            std::cout << " " << cpu_id;
        }
        std::cout << "\n";
    }

    // CPU query methods
    {
        const auto& pkg0 = sys.info.cpu.packages.front();
        assert(sys.info.cpu.find_package(pkg0.id) != nullptr);
        label("find_package(" + std::to_string(pkg0.id.value()) + ")", "OK");

        const auto& core0 = sys.info.cpu.cores.front();
        assert(sys.info.cpu.find_core(core0.id) != nullptr);
        label("find_core(" + std::to_string(core0.id.value()) + ")", "OK");

        const auto& lcpu0 = sys.info.cpu.logical_cpus.front();
        assert(sys.info.cpu.find_logical_cpu(lcpu0.id) != nullptr);
        label("find_logical_cpu(" + std::to_string(lcpu0.id.value()) + ")", "OK");

        auto lcpus_pkg = sys.info.cpu.logical_cpus_of_package(pkg0.id);
        assert(!lcpus_pkg.empty());
        label("logical_cpus_of_package(" + std::to_string(pkg0.id.value()) + ")",
              std::to_string(lcpus_pkg.size()));

        auto cores_pkg = sys.info.cpu.cores_of_package(pkg0.id);
        assert(!cores_pkg.empty());
        label("cores_of_package(" + std::to_string(pkg0.id.value()) + ")",
              std::to_string(cores_pkg.size()));

        auto vis = sys.info.cpu.visible_logical_cpus();
        assert(!vis.empty());
        label("visible_logical_cpus()", std::to_string(vis.size()));
    }

    // ── 4. Memory ───────────────────────────────────────────────────────
    section("4. Memory");
    label("Total", format_memory(sys.info.memory.total_memory.value));
    if(sys.info.memory.available_memory.has_value())
    {
        label("Available", format_memory(sys.info.memory.available_memory->value));
    }
    if(!sys.info.memory.numa_memory.empty())
    {
        for(const auto& nm : sys.info.memory.numa_memory)
        {
            label("NUMA " + std::to_string(nm.node.value()) + " total",
                  format_memory(nm.total.value));
            if(nm.available.has_value())
            {
                label("NUMA " + std::to_string(nm.node.value()) + " available",
                      format_memory(nm.available->value));
            }
        }
    }

    // ── 5. Accelerators ─────────────────────────────────────────────────
    section("5. Accelerators");
    label("Devices", sys.info.accelerators.devices.size());
    {
        auto gpus = sys.info.accelerators.gpus();
        auto npus = sys.info.accelerators.npus();
        auto fpgas = sys.info.accelerators.fpgas();
        label("GPUs", gpus.size());
        label("NPUs", npus.size());
        label("FPGAs", fpgas.size());

        for(const auto* gpu : gpus)
        {
            std::cout << "  GPU [" << gpu->id << "] " << gpu->name.value << "\n";
            label("  Vendor", gpu->vendor.value);
            label("  Kind", accel_kind_str(gpu->kind));
            if(gpu->memory_size.has_value())
            {
                label("  Memory", format_memory(gpu->memory_size->value));
            }
            if(gpu->pci_address.has_value())
            {
                label("  PCI", format_pci(*gpu->pci_address));
            }
            if(gpu->nearest_numa_node.has_value())
            {
                label("  NUMA node", std::to_string(gpu->nearest_numa_node->value()));
            }
            label("  Visible", gpu->visible_to_current_process ? "yes" : "no");
        }

        auto vis = sys.info.accelerators.visible();
        label("Visible accelerators", vis.size());

        if(!sys.info.accelerators.devices.empty())
        {
            const auto& dev0 = sys.info.accelerators.devices.front();
            assert(sys.info.accelerators.find(dev0.id) != nullptr);
            label("find(" + std::to_string(dev0.id.value()) + ")", "OK");
        }
    }

    // ── 6. Network ──────────────────────────────────────────────────────
    section("6. Network");
    label("Interfaces", sys.info.network.interfaces.size());
    for(const auto& iface : sys.info.network.interfaces)
    {
        std::cout << "  " << iface.name.value << " (" << state_str(iface.state) << ")\n";
        if(!iface.mac.value.empty())
        {
            label("  MAC", iface.mac.value);
        }
        if(iface.speed.has_value())
        {
            label("  Speed (Mbps)", iface.speed->value / 1000000);
        }
        if(!iface.addresses.empty())
        {
            std::string ips;
            for(const auto& addr : iface.addresses)
            {
                if(!ips.empty())
                {
                    ips += ", ";
                }
                ips += addr.value;
            }
            label("  IPs", ips);
        }
        if(iface.pci_address.has_value())
        {
            label("  PCI", format_pci(*iface.pci_address));
        }
        label("  Visible", iface.visible_to_current_process ? "yes" : "no");
    }
    {
        auto vis = sys.info.network.visible();
        label("Visible interfaces", vis.size());

        if(!sys.info.network.interfaces.empty())
        {
            const auto& if0 = sys.info.network.interfaces.front();
            assert(sys.info.network.find(if0.name) != nullptr);
            label("find(" + if0.name.value + ")", "OK");
        }
    }

    // ── 7. Storage ──────────────────────────────────────────────────────
    section("7. Storage");
    label("Devices", sys.info.storage.devices.size());
    for(const auto& dev : sys.info.storage.devices)
    {
        std::cout << "  " << dev.name.value << " (" << storage_kind_str(dev.kind) << ")\n";
        if(dev.capacity.has_value())
        {
            label("  Capacity", format_memory(dev.capacity->value));
        }
        if(dev.pci_address.has_value())
        {
            label("  PCI", format_pci(*dev.pci_address));
        }
    }

    // ── 8. PCI ──────────────────────────────────────────────────────────
    section("8. PCI");
    label("Devices", sys.info.pci.devices.size());
    {
        std::size_t shown = 0;
        for(const auto& dev : sys.info.pci.devices)
        {
            if(shown >= 10)
            {
                break;
            }
            label(format_pci(dev.address), dev.vendor.value + " | " + dev.device_name.value +
                                               " | " + dev.device_class.value);
            if(dev.numa_node.has_value())
            {
                label("  NUMA node", std::to_string(dev.numa_node->value()));
            }
            ++shown;
        }
        if(!sys.info.pci.devices.empty())
        {
            const auto& pci0 = sys.info.pci.devices.front();
            assert(sys.info.pci.find(pci0.address) != nullptr);
            label("find(" + format_pci(pci0.address) + ")", "OK");
        }
    }

    // ── 9. Software ─────────────────────────────────────────────────────
    section("9. Software");
    label("Drivers", sys.info.software.drivers.size());
    for(const auto& drv : sys.info.software.drivers)
    {
        label("  " + drv.name, drv.version + (drv.loaded ? " (loaded)" : " (not loaded)"));
    }
    label("Runtimes", sys.info.software.runtimes.size());
    for(const auto& rt : sys.info.software.runtimes)
    {
        label("  " + rt.name, rt.version);
    }
    label("Compilers", sys.info.software.compilers.size());
    for(const auto& cc : sys.info.software.compilers)
    {
        label("  " + cc.name, cc.version);
    }
    label("Libraries", sys.info.software.libraries.size());
    for(const auto& lib : sys.info.software.libraries)
    {
        label("  " + lib.name, lib.version + " [" + lib.kind + "]");
    }
    if(sys.info.software.cuda.has_value())
    {
        label("CUDA version", sys.info.software.cuda->version);
        label("CUDA driver", sys.info.software.cuda->driver_version);
        label("nvcc path", sys.info.software.cuda->nvcc_path);
        label("CUDA_HOME", sys.info.software.cuda->home);
    }
    if(sys.info.software.rocm.has_value())
    {
        label("ROCm version", sys.info.software.rocm->version);
        label("HIP path", sys.info.software.rocm->hip_path);
        label("ROCm path", sys.info.software.rocm->rocm_path);
    }
    if(sys.info.software.level_zero.has_value())
    {
        label("LevelZero version", sys.info.software.level_zero->version);
        label("LevelZero loader", sys.info.software.level_zero->loader_path);
    }
    if(sys.info.software.mpi.has_value())
    {
        label("MPI implementation", sys.info.software.mpi->implementation);
        label("MPI version", sys.info.software.mpi->version);
    }
    if(sys.info.software.rdma.has_value())
    {
        label("RDMA core version", sys.info.software.rdma->rdma_core_version);
    }

    // ── 10. Execution Context ───────────────────────────────────────────
    section("10. Execution Context");
    label("PID", sys.info.execution.process.pid);
    label("PPID", sys.info.execution.process.ppid);
    label("UID", sys.info.execution.process.uid);
    label("GID", sys.info.execution.process.gid);
    label("Comm", sys.info.execution.process.comm);
    label("Exe", sys.info.execution.process.exe);
    label("Cwd", sys.info.execution.process.cwd);
    label("EUID", sys.info.execution.permission.euid);
    label("EGID", sys.info.execution.permission.egid);
    label("Is root", sys.info.execution.permission.is_root ? "yes" : "no");
    if(!sys.info.execution.permission.capabilities.empty())
    {
        std::string caps;
        for(const auto& cap : sys.info.execution.permission.capabilities)
        {
            if(!caps.empty())
            {
                caps += ", ";
            }
            caps += cap;
        }
        label("Capabilities", caps);
    }
    label("Cgroup version", cgroup_ver_str(sys.info.execution.cgroup.version));
    label("Cgroup path", sys.info.execution.cgroup.path);
    if(!sys.info.execution.cgroup.controllers.empty())
    {
        std::string ctrls;
        for(const auto& c : sys.info.execution.cgroup.controllers)
        {
            if(!ctrls.empty())
            {
                ctrls += ", ";
            }
            ctrls += c;
        }
        label("Cgroup controllers", ctrls);
    }
    label("Cpuset cpus", sys.info.execution.cpuset.cpus);
    label("Cpuset mems", sys.info.execution.cpuset.mems);
    label("Cpuset cpus_effective", sys.info.execution.cpuset.cpus_effective);
    label("Cpuset mems_effective", sys.info.execution.cpuset.mems_effective);
    if(sys.info.execution.container.has_value())
    {
        label("Container kind", container_kind_str(sys.info.execution.container->kind));
        label("Container id", sys.info.execution.container->id);
        label("Container runtime", sys.info.execution.container->runtime);
    }
    else
    {
        label("Container", "none");
    }
    label("Visible CPU IDs", sys.info.execution.visible_logical_cpu_ids.size());
    label("Visible accelerator IDs", sys.info.execution.visible_accelerator_ids.size());
    label("Visible interface names", sys.info.execution.visible_network_interface_names.size());

    // ── 11. Visibility ──────────────────────────────────────────────────
    section("11. Visibility");
    {
        auto vis_cpus = sys.info.cpu.visible_logical_cpus();
        label("Visible logical CPUs", vis_cpus.size());
        for(const auto* cpu : vis_cpus)
        {
            std::cout << "    CPU " << cpu->id << " core=" << cpu->core_id
                      << " pkg=" << cpu->package_id;
            if(cpu->numa_node.has_value())
            {
                std::cout << " numa=" << *cpu->numa_node;
            }
            std::cout << "\n";
        }
    }
    {
        auto vis_acc = sys.info.accelerators.visible();
        label("Visible accelerators", vis_acc.size());
        for(const auto* acc : vis_acc)
        {
            std::cout << "    [" << acc->id << "] " << acc->name.value << "\n";
        }
    }
    {
        auto vis_if = sys.info.network.visible();
        label("Visible interfaces", vis_if.size());
        for(const auto* iface : vis_if)
        {
            std::cout << "    " << iface->name.value << "\n";
        }
    }

    // ── 12. Raw Store ───────────────────────────────────────────────────
    section("12. Raw Store");
    if(sys.raw.has_value())
    {
        const auto& raw = *sys.raw;
        label("Total records", raw.records.size());

        // Breakdown by source
        std::cout << "  Sources breakdown:\n";
        for(int i = 0; i <= static_cast<int>(sysal::RawSource::HwinfoOutput); ++i)
        {
            const auto src = static_cast<sysal::RawSource>(i);
            const auto cnt = raw.count(src);
            if(cnt > 0)
            {
                std::cout << "    " << std::left << std::setw(20) << raw_source_str(src) << ": "
                          << cnt << "\n";
            }
        }

        // First few record paths
        std::cout << "  First records:\n";
        std::size_t shown = 0;
        for(const auto& rec : raw.records)
        {
            if(shown >= 10)
            {
                break;
            }
            std::cout << "    [" << raw_source_str(rec.source) << "] " << rec.path_or_command
                      << " (" << collect_status_str(rec.status) << ")\n";
            ++shown;
        }
    }
    else
    {
        label("Raw store", "not collected");
    }

    // ── 13. Warnings & Meta ─────────────────────────────────────────────
    section("13. Warnings & Meta");
    label("Warnings", sys.warnings.size());
    for(const auto& w : sys.warnings)
    {
        std::cout << "    " << w << "\n";
    }
    label("sysal_version", sys.meta.sysal_version);
    label("collect_duration (ms)",
          std::to_string(static_cast<long>(sys.meta.collect_duration.count() * 1000.0)));
    label("requested_flags", std::to_string(static_cast<unsigned>(sys.meta.requested_flags)));
    label("succeeded_collectors", sys.meta.succeeded_collectors.size());
    label("failed_collectors", sys.meta.failed_collectors.size());

    // ── 14. JSON Serialization Round-trip ───────────────────────────────
    section("14. JSON Serialization Round-trip");
    {
        auto json = sysal::to_json(sys, {.pretty_print = true});
        label("JSON length", json.size());
        std::cout << "  First 500 chars:\n";
        std::cout << json.substr(0, 500) << "\n";

        auto sys2 = sysal::from_json(json);
        assert(sys2.info.cpu.logical_cpus.size() == sys.info.cpu.logical_cpus.size());
        assert(sys2.info.memory.total_memory.value == sys.info.memory.total_memory.value);
        assert(sys2.info.platform.host.hostname == sys.info.platform.host.hostname);
        label("CPU count match", "OK");
        label("Memory total match", "OK");
        label("Hostname match", "OK");
        std::cout << "  Round-trip OK\n";
    }

    // ── 15. Partial Collection ──────────────────────────────────────────
    section("15. Partial Collection");
    {
        auto partial = sysal::System::collect(sysal::Collect::Cpu | sysal::Collect::Memory);
        assert(!partial.info.cpu.logical_cpus.empty());
        assert(partial.info.memory.total_memory.value > 0);
        label("CPU logical CPUs", partial.info.cpu.logical_cpus.size());
        label("Memory total", format_memory(partial.info.memory.total_memory.value));
        label("Platform hostname", partial.info.platform.host.hostname.empty()
                                       ? "(empty)"
                                       : partial.info.platform.host.hostname);
        label("Network interfaces", partial.info.network.interfaces.size());
        std::cout << "  Unrequested domains are default-constructed\n";
    }

    // ── 16. Refresh ─────────────────────────────────────────────────────
    section("16. Refresh");
    {
        const auto cpu_count_before = sys.info.cpu.logical_cpus.size();
        const auto mem_before = sys.info.memory.total_memory.value;
        sys.refresh();
        assert(!sys.info.cpu.logical_cpus.empty());
        assert(sys.info.memory.total_memory.value > 0);
        label("CPU count before", cpu_count_before);
        label("CPU count after", sys.info.cpu.logical_cpus.size());
        label("Memory before", format_memory(mem_before));
        label("Memory after", format_memory(sys.info.memory.total_memory.value));
        assert(sys.info.cpu.logical_cpus.size() == cpu_count_before);
        std::cout << "  Refresh OK\n";
    }

    // ── 17. Error Handling ──────────────────────────────────────────────
    section("17. Error Handling");
    {
        sysal::RawStore empty_raw{};
        try
        {
            (void)sysal::test::collect_from_raw(empty_raw, sysal::Collect::Cpu);
            label("Expected SysalError", "NOT thrown");
        }
        catch(const sysal::SysalError& e)
        {
            label("Caught SysalError", e.what());
            label("Error kind",
                  e.kind() == sysal::ErrorKind::CollectionFailed ? "CollectionFailed" : "other");
            assert(e.kind() == sysal::ErrorKind::CollectionFailed);
        }
    }

    std::cout << "\n=== testbench done ===\n";
    return 0;
}
