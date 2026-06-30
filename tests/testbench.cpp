/// @file testbench.cpp
/// @brief sysal 全量 API 演示
/// @details 采集当前机器全部系统信息，以格式化文本输出各子域数据，
///          并演示 JSON 序列化与 refresh 功能。

#include <sysal/core/sysal.hpp>
#include <sysal/serialization/serialization.hpp>

#include <iomanip>
#include <iostream>

namespace
{

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

/// @brief InterfaceState 转字符串
/// @param state 接口状态
/// @return 状态名称
const char* state_str(sysal::InterfaceState state)
{
    switch(state)
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

/// @brief StorageKind 转字符串
/// @param kind 存储类型
/// @return 类型名称
const char* storage_kind_str(sysal::StorageKind kind)
{
    switch(kind)
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

/// @brief AcceleratorKind 转字符串
/// @param kind 加速器类型
/// @return 类型名称
const char* accel_kind_str(sysal::AcceleratorKind kind)
{
    switch(kind)
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

} // namespace

int main()
{
    std::cout << "=== sysal testbench ===\n\n";

    auto sys = sysal::System::collect();

    // Platform
    std::cout << "Platform:\n";
    std::cout << "  Hostname: " << sys.info.platform.host.hostname << "\n";
    std::cout << "  OS: " << sys.info.platform.os.name << "\n";
    if(!sys.info.platform.os.distribution.empty())
    {
        std::cout << "  Distribution: " << sys.info.platform.os.distribution << " "
                  << sys.info.platform.os.distribution_version << "\n";
    }
    std::cout << "  Kernel: " << sys.info.platform.kernel.release << "\n";
    std::cout << "  Arch: " << sys.info.platform.architecture.name << " ("
              << sys.info.platform.architecture.bits << "-bit, "
              << sys.info.platform.architecture.byte_order << ")\n";
    if(sys.info.platform.firmware.has_value())
    {
        std::cout << "  BIOS: " << sys.info.platform.firmware->bios_vendor << " "
                  << sys.info.platform.firmware->bios_version << " ("
                  << sys.info.platform.firmware->bios_date << ")\n";
    }
    if(sys.info.platform.virtualization.has_value())
    {
        std::cout << "  Virtualization: container="
                  << (sys.info.platform.virtualization->container ? "yes" : "no") << "\n";
    }

    // CPU
    std::cout << "\nCPU:\n";
    std::cout << "  Packages: " << sys.info.cpu.packages.size() << "\n";
    std::cout << "  Cores: " << sys.info.cpu.cores.size() << "\n";
    std::cout << "  Logical CPUs: " << sys.info.cpu.logical_cpus.size() << "\n";
    std::cout << "  NUMA nodes: " << sys.info.cpu.numa_nodes.size() << "\n";
    for(const auto& pkg : sys.info.cpu.packages)
    {
        std::cout << "  Package " << pkg.id << ": " << pkg.model_name.value << " ("
                  << pkg.physical_cores << " cores, " << pkg.logical_threads << " threads)\n";
        if(pkg.base_frequency.has_value())
        {
            std::cout << "    Base freq: " << pkg.base_frequency->value / 1000 << " MHz\n";
        }
        if(pkg.max_frequency.has_value())
        {
            std::cout << "    Max freq: " << pkg.max_frequency->value / 1000 << " MHz\n";
        }
    }
    if(!sys.info.cpu.isa_extensions.empty())
    {
        std::cout << "  ISA extensions: " << sys.info.cpu.isa_extensions.size() << "\n";
    }

    // Memory
    std::cout << "\nMemory:\n";
    std::cout << "  Total: " << format_memory(sys.info.memory.total_memory.value) << "\n";
    if(sys.info.memory.available_memory.has_value())
    {
        std::cout << "  Available: " << format_memory(sys.info.memory.available_memory->value)
                  << "\n";
    }
    if(!sys.info.memory.numa_memory.empty())
    {
        std::cout << "  NUMA memory:\n";
        for(const auto& nm : sys.info.memory.numa_memory)
        {
            std::cout << "    Node " << nm.node << ": " << format_memory(nm.total.value) << "\n";
        }
    }

    // Accelerators
    std::cout << "\nAccelerators:\n";
    std::cout << "  Devices: " << sys.info.accelerators.devices.size() << "\n";
    auto gpus = sys.info.accelerators.gpus();
    std::cout << "  GPUs: " << gpus.size() << "\n";
    for(const auto* gpu : gpus)
    {
        std::cout << "    [" << gpu->id << "] " << gpu->name.value << " ("
                  << accel_kind_str(gpu->kind) << ")\n";
        if(gpu->memory_size.has_value())
        {
            std::cout << "      Memory: " << format_memory(gpu->memory_size->value) << "\n";
        }
        if(gpu->pci_address.has_value())
        {
            std::cout << "      PCI: " << format_pci(*gpu->pci_address) << "\n";
        }
        if(gpu->nearest_numa_node.has_value())
        {
            std::cout << "      NUMA node: " << gpu->nearest_numa_node->value() << "\n";
        }
    }

    // Network
    std::cout << "\nNetwork:\n";
    std::cout << "  Interfaces: " << sys.info.network.interfaces.size() << "\n";
    for(const auto& iface : sys.info.network.interfaces)
    {
        std::cout << "  " << iface.name.value << " (" << state_str(iface.state) << ")\n";
        if(!iface.mac.value.empty())
        {
            std::cout << "    MAC: " << iface.mac.value << "\n";
        }
        if(iface.speed.has_value())
        {
            std::cout << "    Speed: " << iface.speed->value / 1000000 << " Mbps\n";
        }
        if(!iface.addresses.empty())
        {
            std::cout << "    IPs:";
            for(const auto& addr : iface.addresses)
            {
                std::cout << " " << addr.value;
            }
            std::cout << "\n";
        }
        if(iface.pci_address.has_value())
        {
            std::cout << "    PCI: " << format_pci(*iface.pci_address) << "\n";
        }
    }

    // PCI
    std::cout << "\nPCI:\n";
    std::cout << "  Devices: " << sys.info.pci.devices.size() << "\n";

    // Storage
    std::cout << "\nStorage:\n";
    std::cout << "  Devices: " << sys.info.storage.devices.size() << "\n";
    for(const auto& dev : sys.info.storage.devices)
    {
        std::cout << "  " << dev.name.value << " (" << storage_kind_str(dev.kind) << ")\n";
        if(dev.capacity.has_value())
        {
            std::cout << "    Capacity: " << format_memory(dev.capacity->value) << "\n";
        }
        if(dev.pci_address.has_value())
        {
            std::cout << "    PCI: " << format_pci(*dev.pci_address) << "\n";
        }
    }

    // Software
    std::cout << "\nSoftware:\n";
    std::cout << "  Drivers: " << sys.info.software.drivers.size() << "\n";
    for(const auto& drv : sys.info.software.drivers)
    {
        std::cout << "    " << drv.name << " " << drv.version
                  << (drv.loaded ? " (loaded)" : " (not loaded)") << "\n";
    }
    std::cout << "  Runtimes: " << sys.info.software.runtimes.size() << "\n";
    if(sys.info.software.cuda.has_value())
    {
        std::cout << "  CUDA: " << sys.info.software.cuda->version << "\n";
        std::cout << "  CUDA driver: " << sys.info.software.cuda->driver_version << "\n";
    }
    if(sys.info.software.rocm.has_value())
    {
        std::cout << "  ROCm: " << sys.info.software.rocm->version << "\n";
    }

    // Execution
    std::cout << "\nExecution:\n";
    std::cout << "  PID: " << sys.info.execution.process.pid << "\n";
    std::cout << "  UID: " << sys.info.execution.process.uid << "\n";
    std::cout << "  EUID: " << sys.info.execution.permission.euid << "\n";
    std::cout << "  Is root: " << (sys.info.execution.permission.is_root ? "yes" : "no") << "\n";
    std::cout << "  Visible CPUs: " << sys.info.execution.visible_logical_cpu_ids.size() << "\n";
    std::cout << "  Visible accelerators: " << sys.info.execution.visible_accelerator_ids.size()
              << "\n";
    if(sys.info.execution.container.has_value())
    {
        std::cout << "  Container: yes\n";
    }

    // Warnings
    std::cout << "\nWarnings: " << sys.warnings.size() << "\n";
    for(const auto& w : sys.warnings)
    {
        std::cout << "  " << w << "\n";
    }

    // Meta
    std::cout << "\nMeta:\n";
    std::cout << "  Version: " << sys.meta.sysal_version << "\n";
    std::cout << "  Duration: " << std::fixed << std::setprecision(3)
              << sys.meta.collect_duration.count() << "s\n";
    std::cout << "  Succeeded: " << sys.meta.succeeded_collectors.size() << " collectors\n";
    std::cout << "  Failed: " << sys.meta.failed_collectors.size() << " collectors\n";

    // JSON serialization
    std::cout << "\n--- JSON (pretty, first 500 chars) ---\n";
    auto json = sysal::to_json(sys, {.pretty_print = true});
    std::cout << json.substr(0, 500) << "...\n";

    // Refresh
    std::cout << "\n--- Refresh ---\n";
    sys.refresh();
    std::cout << "After refresh: " << sys.info.cpu.logical_cpus.size() << " CPUs, "
              << format_memory(sys.info.memory.total_memory.value) << " memory\n";

    std::cout << "\n=== testbench done ===\n";
    return 0;
}
