#include "sysal/collect.hpp"

#include <iostream>

int main()
{
    auto snapshot = sysal::collect_or_throw(sysal::CollectSpec::full().with_raw());

    std::cout << "=== Platform ===\n";
    std::cout << "Hostname: " << snapshot.platform.host.hostname << "\n";
    std::cout << "OS: " << snapshot.platform.os.name << " " << snapshot.platform.os.version << "\n";
    std::cout << "Kernel: " << snapshot.platform.kernel.release << "\n";

    std::cout << "\n=== CPU ===\n";
    std::cout << "Packages: " << snapshot.resources.cpu.packages.size() << "\n";
    std::cout << "Logical CPUs: " << snapshot.resources.cpu.logical_cpus.size() << "\n";
    std::cout << "ISA extensions: " << snapshot.resources.cpu.isa_extensions.size() << "\n";
    std::cout << "NUMA nodes: " << snapshot.resources.cpu.numa_nodes.size() << "\n";

    std::cout << "\n=== Memory ===\n";
    std::cout << "Total: " << snapshot.resources.memory.total_memory.value << " bytes\n";
    if(snapshot.resources.memory.available_memory)
    {
        std::cout << "Available: " << snapshot.resources.memory.available_memory->value
                  << " bytes\n";
    }
    std::cout << "NUMA memory entries: " << snapshot.resources.memory.numa_memory.size() << "\n";

    std::cout << "\n=== Network ===\n";
    std::cout << "Interfaces: " << snapshot.resources.network.interfaces.size() << "\n";
    for(const auto& iface : snapshot.resources.network.interfaces)
    {
        std::cout << "  " << iface.name.value << " state=" << static_cast<int>(iface.state);
        if(iface.pci_address)
        {
            std::cout << " pci=" << iface.pci_address->domain << ":"
                      << static_cast<int>(iface.pci_address->bus) << ":"
                      << static_cast<int>(iface.pci_address->device) << "."
                      << static_cast<int>(iface.pci_address->function);
        }
        std::cout << " ips=" << iface.addresses.size() << "\n";
    }

    std::cout << "\n=== PCI ===\n";
    std::cout << "Devices: " << snapshot.resources.pci.devices.size() << "\n";

    std::cout << "\n=== Storage ===\n";
    std::cout << "Devices: " << snapshot.resources.storage.devices.size() << "\n";
    for(const auto& dev : snapshot.resources.storage.devices)
    {
        std::cout << "  " << dev.name.value;
        if(dev.capacity)
        {
            std::cout << " size=" << dev.capacity->value << " bytes";
        }
        std::cout << "\n";
    }

    std::cout << "\n=== Accelerators ===\n";
    std::cout << "Devices: " << snapshot.resources.accelerators.devices.size() << "\n";
    for(const auto& dev : snapshot.resources.accelerators.devices)
    {
        std::cout << "  " << dev.name.value << " vendor=" << dev.vendor.value;
        if(dev.memory_size)
        {
            std::cout << " mem=" << dev.memory_size->value << " bytes";
        }
        std::cout << "\n";
    }

    std::cout << "\n=== Topology ===\n";
    std::cout << "NUMA relations: " << snapshot.resources.topology.numa_relations.size() << "\n";
    std::cout << "Device localities: " << snapshot.resources.topology.device_localities.size()
              << "\n";

    std::cout << "\n=== Software ===\n";
    if(snapshot.software.cuda)
    {
        std::cout << "CUDA driver: " << snapshot.software.cuda->driver_version << "\n";
        std::cout << "CUDA runtime: " << snapshot.software.cuda->runtime_version << "\n";
        std::cout << "CUDA devices: " << snapshot.software.cuda->device_count << "\n";
    }
    std::cout << "Drivers: " << snapshot.software.drivers.size() << "\n";

    std::cout << "\n=== Execution ===\n";
    std::cout << "PID: " << snapshot.execution.process.pid << "\n";
    std::cout << "Cgroup version: " << static_cast<int>(snapshot.execution.cgroup.version) << "\n";
    std::cout << "Cpuset restricted: " << snapshot.execution.cpuset.is_restricted << "\n";
    std::cout << "Visible CPUs: " << snapshot.execution.visible_logical_cpu_ids.size() << "\n";
    if(snapshot.execution.container)
    {
        std::cout << "Container: " << static_cast<int>(snapshot.execution.container->kind) << "\n";
    }
    std::cout << "Env vars: " << snapshot.execution.environment.relevant_vars.size() << "\n";

    std::cout << "\n=== Meta ===\n";
    std::cout << "sysal version: " << snapshot.meta.sysal_version << "\n";
    std::cout << "Succeeded collectors: " << snapshot.meta.succeeded_collectors.size() << "\n";
    for(const auto& name : snapshot.meta.succeeded_collectors)
    {
        std::cout << "  " << name << "\n";
    }
    std::cout << "Diagnostics: " << snapshot.diagnostics.records.size() << " records\n";

    return 0;
}
