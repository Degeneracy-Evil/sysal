#include "sysal/collect.hpp"
#include "sysal/serialization.hpp"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>

namespace
{

std::string format_bytes(std::uint64_t bytes)
{
    constexpr std::uint64_t kib = 1024;
    constexpr std::uint64_t mib = kib * 1024;
    constexpr std::uint64_t gib = mib * 1024;
    constexpr std::uint64_t tib = gib * 1024;

    if(bytes >= tib)
    {
        return std::to_string(bytes / tib) + " TiB";
    }
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

std::string format_pci(const sysal::PciAddress& addr)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04x:%02x:%02x.%x", addr.domain, addr.bus, addr.device,
                  addr.function);
    return buf;
}

std::string format_bps(std::uint64_t bps)
{
    constexpr std::uint64_t kbps = 1000;
    constexpr std::uint64_t mbps = kbps * 1000;
    constexpr std::uint64_t gbps = mbps * 1000;

    if(bps >= gbps)
    {
        return std::to_string(bps / gbps) + " Gbps";
    }
    if(bps >= mbps)
    {
        return std::to_string(bps / mbps) + " Mbps";
    }
    if(bps >= kbps)
    {
        return std::to_string(bps / kbps) + " Kbps";
    }
    return std::to_string(bps) + " bps";
}

std::string format_gib(std::uint64_t bytes)
{
    constexpr std::uint64_t gib = 1024ULL * 1024ULL * 1024ULL;
    return std::to_string(bytes / gib) + " GiB";
}

std::string arch_to_string(sysal::Architecture arch)
{
    switch(arch)
    {
    case sysal::Architecture::X86_64:
        return "x86_64";
    case sysal::Architecture::AArch64:
        return "aarch64";
    case sysal::Architecture::Riscv64:
        return "riscv64";
    default:
        return "other";
    }
}

std::string isa_to_string(sysal::IsaExtension ext)
{
    switch(ext)
    {
    case sysal::IsaExtension::Sse42:
        return "SSE4.2";
    case sysal::IsaExtension::Avx:
        return "AVX";
    case sysal::IsaExtension::Avx2:
        return "AVX2";
    case sysal::IsaExtension::Avx512f:
        return "AVX512F";
    case sysal::IsaExtension::Avx512cd:
        return "AVX512CD";
    case sysal::IsaExtension::Avx512bw:
        return "AVX512BW";
    case sysal::IsaExtension::Avx512dq:
        return "AVX512DQ";
    case sysal::IsaExtension::Avx512vl:
        return "AVX512VL";
    case sysal::IsaExtension::Neon:
        return "Neon";
    case sysal::IsaExtension::Sve:
        return "SVE";
    case sysal::IsaExtension::Sve2:
        return "SVE2";
    case sysal::IsaExtension::AmxInt8:
        return "AMX-INT8";
    default:
        return "unknown";
    }
}

std::string state_to_string(sysal::InterfaceState state)
{
    switch(state)
    {
    case sysal::InterfaceState::Up:
        return "up";
    case sysal::InterfaceState::Down:
        return "down";
    default:
        return "unknown";
    }
}

std::string storage_kind_to_string(sysal::StorageKind kind)
{
    switch(kind)
    {
    case sysal::StorageKind::Nvme:
        return "NVMe";
    case sysal::StorageKind::Sata:
        return "SATA";
    case sysal::StorageKind::Sas:
        return "SAS";
    default:
        return "other";
    }
}

std::string container_kind_to_string(sysal::ContainerKind kind)
{
    switch(kind)
    {
    case sysal::ContainerKind::Docker:
        return "Docker";
    case sysal::ContainerKind::Podman:
        return "Podman";
    case sysal::ContainerKind::Lxc:
        return "LXC";
    case sysal::ContainerKind::Kubernetes:
        return "Kubernetes";
    default:
        return "other";
    }
}

void print_header(const std::string& title)
{
    std::cout << "\n========================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "========================================\n";
}

void print_platform(const sysal::PlatformInfo& p)
{
    print_header("Platform");
    std::cout << "  Hostname:       " << p.host.hostname << "\n";
    std::cout << "  OS:             " << p.os.name << " " << p.os.version << "\n";
    std::cout << "  Kernel:         " << p.kernel.release << "\n";
    std::cout << "  Kernel version: " << p.kernel.version << "\n";
    std::cout << "  Architecture:   " << arch_to_string(p.architecture.cpu_arch) << " ("
              << p.architecture.machine_arch << ")\n";

    if(p.firmware)
    {
        std::cout << "  BIOS vendor:    " << p.firmware->bios_vendor << "\n";
        std::cout << "  BIOS version:   " << p.firmware->bios_version << "\n";
        std::cout << "  BIOS date:      " << p.firmware->bios_date << "\n";
    }
    else
    {
        std::cout << "  Firmware:       (not collected)\n";
    }

    if(p.virtualization)
    {
        std::cout << "  Virtualization: " << p.virtualization->hypervisor << "\n";
    }
    else
    {
        std::cout << "  Virtualization: (none detected)\n";
    }
}

void print_cpu(const sysal::CpuSubsystem& cpu)
{
    print_header("CPU");
    std::cout << "  Architecture:     " << arch_to_string(cpu.arch) << "\n";
    std::cout << "  Packages:         " << cpu.packages.size() << "\n";
    for(const auto& pkg : cpu.packages)
    {
        std::cout << "    Package " << pkg.id.value() << ": " << pkg.vendor.value << " "
                  << pkg.model_name.value << " (" << pkg.physical_cores << " cores, "
                  << pkg.logical_threads << " threads)\n";
        if(pkg.base_frequency)
        {
            std::cout << "      Base freq: " << pkg.base_frequency->value << " Hz\n";
        }
        if(pkg.max_frequency)
        {
            std::cout << "      Max freq:  " << pkg.max_frequency->value << " Hz\n";
        }
    }

    std::cout << "  Cores:            " << cpu.cores.size() << "\n";
    std::cout << "  Logical CPUs:     " << cpu.logical_cpus.size() << "\n";
    std::cout << "  NUMA nodes:       " << cpu.numa_nodes.size() << "\n";

    std::cout << "  ISA extensions:   ";
    for(auto ext : cpu.isa_extensions)
    {
        std::cout << isa_to_string(ext) << " ";
    }
    std::cout << "\n";

    auto visible = cpu.visible_logical_cpus();
    std::cout << "  Visible CPUs:     " << visible.size() << " / " << cpu.logical_cpus.size()
              << "\n";
}

void print_memory(const sysal::MemorySubsystem& mem)
{
    print_header("Memory");
    std::cout << "  Total:     " << format_bytes(mem.total_memory.value) << "\n";
    if(mem.available_memory)
    {
        std::cout << "  Available: " << format_bytes(mem.available_memory->value) << "\n";
    }
    std::cout << "  NUMA nodes: " << mem.numa_memory.size() << "\n";
    for(const auto& nm : mem.numa_memory)
    {
        std::cout << "    Node " << nm.node.value() << ": total=" << format_bytes(nm.total.value);
        if(nm.available)
        {
            std::cout << " available=" << format_bytes(nm.available->value);
        }
        std::cout << "\n";
    }
}

void print_accelerators(const sysal::AcceleratorSubsystem& acc)
{
    print_header("Accelerators");
    std::cout << "  Devices: " << acc.devices.size() << "\n";
    for(const auto& dev : acc.devices)
    {
        std::cout << "    [" << dev.id.value() << "] " << dev.vendor.value << " " << dev.name.value
                  << "\n";
        if(dev.memory_size)
        {
            std::cout << "      Memory:    " << format_bytes(dev.memory_size->value) << "\n";
        }
        if(dev.pci_address)
        {
            std::cout << "      PCI:       " << format_pci(*dev.pci_address) << "\n";
        }
        if(dev.nearest_numa_node)
        {
            std::cout << "      NUMA node: " << dev.nearest_numa_node->value() << "\n";
        }
        std::cout << "      Visible:   " << (dev.visible_to_current_process ? "yes" : "no") << "\n";
    }
}

void print_network(const sysal::NetworkSubsystem& net)
{
    print_header("Network");
    std::cout << "  Interfaces: " << net.interfaces.size() << "\n";
    for(const auto& iface : net.interfaces)
    {
        std::cout << "    " << iface.name.value << "  state=" << state_to_string(iface.state)
                  << "  mac=" << iface.mac.value << "\n";
        if(iface.speed)
        {
            std::cout << "      Speed: " << format_bps(iface.speed->value) << "\n";
        }
        for(const auto& addr : iface.addresses)
        {
            std::cout << "      IP: " << addr.value << "\n";
        }
        if(iface.pci_address)
        {
            std::cout << "      PCI: " << format_pci(*iface.pci_address) << "\n";
        }
        std::cout << "      Visible: " << (iface.visible_to_current_process ? "yes" : "no") << "\n";
    }
}

void print_pci(const sysal::PciSubsystem& pci)
{
    print_header("PCI Devices");
    std::cout << "  Total devices: " << pci.devices.size() << "\n";
    std::size_t shown = 0;
    for(const auto& dev : pci.devices)
    {
        if(shown >= 20)
        {
            std::cout << "  ... and " << (pci.devices.size() - 20) << " more\n";
            break;
        }
        std::cout << "    " << format_pci(dev.address) << "  vendor=" << dev.vendor.value
                  << "  device=" << dev.device_name.value << "  class=" << dev.device_class << "\n";
        ++shown;
    }
}

void print_storage(const sysal::StorageSubsystem& storage)
{
    print_header("Storage");
    std::cout << "  Devices: " << storage.devices.size() << "\n";
    for(const auto& dev : storage.devices)
    {
        std::cout << "    [" << dev.id.value() << "] " << dev.name.value
                  << "  kind=" << storage_kind_to_string(dev.kind);
        if(dev.capacity)
        {
            std::cout << "  capacity=" << format_gib(dev.capacity->value);
        }
        if(dev.pci_address)
        {
            std::cout << "  pci=" << format_pci(*dev.pci_address);
        }
        std::cout << "\n";
    }
}

void print_topology(const sysal::TopologyInfo& topo)
{
    print_header("Topology");
    std::cout << "  NUMA relations: " << topo.numa_relations.size() << "\n";
    for(const auto& rel : topo.numa_relations)
    {
        std::cout << "    Node " << rel.node.value();
        if(rel.local_memory)
        {
            std::cout << "  local_memory=" << format_bytes(rel.local_memory->value);
        }
        if(!rel.packages.empty())
        {
            std::cout << "  packages=[";
            for(std::size_t i = 0; i < rel.packages.size(); ++i)
            {
                if(i > 0)
                {
                    std::cout << ",";
                }
                std::cout << rel.packages[i].value();
            }
            std::cout << "]";
        }
        std::cout << "\n";
    }

    std::cout << "  PCI relations: " << topo.pci_relations.size() << "\n";
    std::cout << "  Device localities: " << topo.device_localities.size() << "\n";
    std::size_t shown = 0;
    for(const auto& loc : topo.device_localities)
    {
        if(shown >= 20)
        {
            std::cout << "    ... and " << (topo.device_localities.size() - 20) << " more\n";
            break;
        }
        std::cout << "    " << format_pci(loc.pci_address) << " -> NUMA node "
                  << loc.nearest_numa_node.value() << "\n";
        ++shown;
    }
}

void print_software(const sysal::SoftwareStackInfo& sw)
{
    print_header("Software Stack");

    std::cout << "  Drivers: " << sw.drivers.size() << "\n";
    for(const auto& drv : sw.drivers)
    {
        std::cout << "    " << drv.name << " " << drv.version
                  << (drv.loaded ? " (loaded)" : " (not loaded)") << "\n";
    }

    std::cout << "  Runtimes: " << sw.runtimes.size() << "\n";
    for(const auto& rt : sw.runtimes)
    {
        std::cout << "    " << rt.name << " " << rt.version;
        if(!rt.path.empty())
        {
            std::cout << "  path=" << rt.path;
        }
        std::cout << "\n";
    }

    std::cout << "  Compilers: " << sw.compilers.size() << "\n";
    std::cout << "  Libraries: " << sw.libraries.size() << "\n";

    if(sw.cuda)
    {
        std::cout << "  CUDA:\n";
        std::cout << "    Driver version:   " << sw.cuda->driver_version << "\n";
        std::cout << "    Runtime version:  " << sw.cuda->runtime_version << "\n";
        std::cout << "    Device count:     " << sw.cuda->device_count << "\n";
    }
    if(sw.rocm)
    {
        std::cout << "  ROCm: " << sw.rocm->version << "\n";
    }
    if(sw.mpi)
    {
        std::cout << "  MPI: " << sw.mpi->implementation << " " << sw.mpi->version << "\n";
    }
    if(sw.rdma)
    {
        std::cout << "  RDMA: ibverbs=" << (sw.rdma->ibverbs_available ? "yes" : "no") << "\n";
    }
}

void print_execution(const sysal::ExecutionContextInfo& exec)
{
    print_header("Execution Context");
    std::cout << "  Process:\n";
    std::cout << "    PID:  " << exec.process.pid << "\n";
    std::cout << "    UID:  " << exec.process.uid << "  EUID: " << exec.process.euid << "\n";
    std::cout << "    GID:  " << exec.process.gid << "  EGID: " << exec.process.egid << "\n";
    std::cout << "    Root: " << (exec.permissions.is_root ? "yes" : "no") << "\n";

    std::cout << "  Cgroup:\n";
    std::cout << "    Version: " << static_cast<int>(exec.cgroup.version) << "\n";
    std::cout << "    Path:    " << exec.cgroup.path << "\n";

    std::cout << "  Cpuset:\n";
    std::cout << "    Restricted: " << (exec.cpuset.is_restricted ? "yes" : "no") << "\n";
    std::cout << "    CPUs:       " << exec.cpuset.cpus.size() << " ids\n";
    std::cout << "    Mems:       " << exec.cpuset.mems.size() << " ids\n";

    if(exec.container)
    {
        std::cout << "  Container: " << container_kind_to_string(exec.container->kind) << "\n";
    }
    else
    {
        std::cout << "  Container: (none)\n";
    }

    std::cout << "  Environment variables:\n";
    for(const auto& [key, val] : exec.environment.relevant_vars)
    {
        std::cout << "    " << key << "=" << val << "\n";
    }

    std::cout << "  Visibility:\n";
    std::cout << "    Visible CPUs:          " << exec.visible_logical_cpu_ids.size() << "\n";
    std::cout << "    Visible accelerators:  " << exec.visible_accelerator_ids.size() << "\n";
    std::cout << "    Visible network ifs:   " << exec.visible_network_interface_names.size()
              << "\n";
}

void print_meta(const sysal::SnapshotMeta& meta)
{
    print_header("Meta");
    std::cout << "  sysal version:    " << meta.sysal_version << "\n";
    std::cout << "  Collect duration: " << meta.collect_duration.count() << " ms\n";
    std::cout << "  Succeeded collectors:\n";
    for(const auto& name : meta.succeeded_collectors)
    {
        std::cout << "    " << name << "\n";
    }
}

void print_diagnostics(const sysal::Diagnostics& diag)
{
    print_header("Diagnostics");
    std::cout << "  Records: " << diag.records.size() << "\n";
    for(const auto& record : diag.records)
    {
        std::cout << "    [" << static_cast<int>(record.severity) << "] " << record.message << "\n";
    }
}

void print_raw(const sysal::SystemSnapshot& snapshot)
{
    print_header("Raw Store");
    if(snapshot.raw)
    {
        std::cout << "  Records: " << snapshot.raw->records.size() << "\n";
        std::unordered_map<std::string, int> by_source;
        for(const auto& rec : snapshot.raw->records)
        {
            by_source[std::to_string(static_cast<int>(rec.source))]++;
        }
        std::cout << "  By source:\n";
        for(const auto& [source, count] : by_source)
        {
            std::cout << "    source " << source << ": " << count << " records\n";
        }
    }
    else
    {
        std::cout << "  (not kept)\n";
    }
}

} // namespace

int main()
{
    std::cout << "╔══════════════════════════════════════════════════╗\n";
    std::cout << "║          sysal testbench — full collection        ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n";

    const auto start = std::chrono::system_clock::now();

    auto snapshot = sysal::collect_or_throw(sysal::CollectSpec::full().with_raw());

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start);

    print_platform(snapshot.platform);
    print_cpu(snapshot.resources.cpu);
    print_memory(snapshot.resources.memory);
    print_accelerators(snapshot.resources.accelerators);
    print_network(snapshot.resources.network);
    print_pci(snapshot.resources.pci);
    print_storage(snapshot.resources.storage);
    print_topology(snapshot.resources.topology);
    print_software(snapshot.software);
    print_execution(snapshot.execution);
    print_meta(snapshot.meta);
    print_diagnostics(snapshot.diagnostics);
    print_raw(snapshot);

    print_header("JSON Serialization");
    auto json = sysal::to_json(snapshot);
    std::cout << "  JSON length: " << json.size() << " chars\n";
    std::cout << "  First 200 chars: " << json.substr(0, 200) << "...\n";

    print_header("Summary");
    std::cout << "  Collection time:    " << elapsed.count() << " ms\n";
    std::cout << "  Succeeded collectors: " << snapshot.meta.succeeded_collectors.size() << "\n";
    std::cout << "  Diagnostics records:  " << snapshot.diagnostics.records.size() << "\n";
    std::cout << "  Raw records:          " << (snapshot.raw ? snapshot.raw->records.size() : 0)
              << "\n";
    std::cout << "  JSON size:            " << json.size() << " chars\n";

    std::cout << "\n  Done.\n";
    return 0;
}
