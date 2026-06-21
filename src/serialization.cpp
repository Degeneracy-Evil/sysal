#include "sysal/serialization.hpp"

#include "detail/json.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

namespace sysal
{
namespace
{

using detail::escape_string;
using detail::JsonArr;
using detail::JsonObj;
using detail::time_point_to_ms;

struct W
{
    bool pretty;

    [[nodiscard]] std::string str(std::string_view s) const { return escape_string(s); }

    template <typename E> [[nodiscard]] std::string en(E e) const
    {
        return std::to_string(static_cast<int>(e));
    }

    [[nodiscard]] std::string u32(std::uint32_t v) const { return std::to_string(v); }

    [[nodiscard]] std::string u64(std::uint64_t v) const { return std::to_string(v); }

    [[nodiscard]] std::string i64(std::int64_t v) const { return std::to_string(v); }

    [[nodiscard]] std::string boolean(bool v) const { return v ? "true" : "false"; }

    template <typename T, typename Tag> [[nodiscard]] std::string id(StrongId<T, Tag> v) const
    {
        return std::to_string(v.value());
    }

    template <typename Tag> [[nodiscard]] std::string named(NamedString<Tag> v) const
    {
        return escape_string(v.value);
    }

    template <typename Tag> [[nodiscard]] std::string unit(ScalarUnit<Tag> v) const
    {
        return std::to_string(v.value);
    }

    [[nodiscard]] std::string pci(const PciAddress& a, int indent) const
    {
        JsonObj o;
        o.add("domain", std::to_string(a.domain));
        o.add("bus", std::to_string(a.bus));
        o.add("device", std::to_string(a.device));
        o.add("function", std::to_string(a.function));
        return o.build(pretty, indent);
    }

    [[nodiscard]] std::string time_ms(std::chrono::system_clock::time_point tp) const
    {
        return time_point_to_ms(tp);
    }
};

std::string to_json_pci(const W& w, const std::optional<PciAddress>& addr, int indent)
{
    if(addr)
    {
        return w.pci(*addr, indent);
    }
    return "null";
}

std::string to_json_collect_spec(const W& w, const CollectSpec& spec, int indent)
{
    JsonObj o;
    o.add("raw", w.boolean(spec.keep_raw()));
    o.add("platform", w.boolean(spec.collect_platform()));
    o.add("cpu", w.boolean(spec.collect_cpu()));
    o.add("memory", w.boolean(spec.collect_memory()));
    o.add("accelerators", w.boolean(spec.collect_accelerators()));
    o.add("network", w.boolean(spec.collect_network()));
    o.add("storage", w.boolean(spec.collect_storage()));
    o.add("pci", w.boolean(spec.collect_pci()));
    o.add("topology", w.boolean(spec.collect_topology()));
    o.add("software_stack", w.boolean(spec.collect_software_stack()));
    o.add("execution_context", w.boolean(spec.collect_execution_context()));
    return o.build(w.pretty, indent);
}

std::string to_json_str_vec(const W& w, const std::vector<std::string>& vec, int indent)
{
    JsonArr a;
    for(const auto& s : vec)
    {
        a.add(w.str(s));
    }
    return a.build(w.pretty, indent);
}

std::string to_json_meta(const W& w, const SnapshotMeta& meta, int indent)
{
    JsonObj o;
    o.add("collect_time", w.time_ms(meta.collect_time));
    o.add("sysal_version", w.str(meta.sysal_version));
    o.add("collect_duration_ms", std::to_string(meta.collect_duration.count()));
    o.add("requested_spec", to_json_collect_spec(w, meta.requested_spec, indent + 1));
    o.add("succeeded_collectors", to_json_str_vec(w, meta.succeeded_collectors, indent + 1));
    o.add("failed_collectors", to_json_str_vec(w, meta.failed_collectors, indent + 1));
    return o.build(w.pretty, indent);
}

std::string to_json_host(const W& w, const HostInfo& host, int indent)
{
    JsonObj o;
    o.add("hostname", w.str(host.hostname));
    return o.build(w.pretty, indent);
}

std::string to_json_os(const W& w, const OsInfo& os, int indent)
{
    JsonObj o;
    o.add("name", w.str(os.name));
    o.add("version", w.str(os.version));
    return o.build(w.pretty, indent);
}

std::string to_json_kernel(const W& w, const KernelInfo& kernel, int indent)
{
    JsonObj o;
    o.add("version", w.str(kernel.version));
    o.add("release", w.str(kernel.release));
    return o.build(w.pretty, indent);
}

std::string to_json_arch_info(const W& w, const ArchitectureInfo& arch, int indent)
{
    JsonObj o;
    o.add("cpu_arch", w.en(arch.cpu_arch));
    o.add("machine_arch", w.str(arch.machine_arch));
    return o.build(w.pretty, indent);
}

std::string to_json_firmware(const W& w, const FirmwareInfo& fw, int indent)
{
    JsonObj o;
    o.add("bios_version", w.str(fw.bios_version));
    o.add("bios_vendor", w.str(fw.bios_vendor));
    o.add("bios_date", w.str(fw.bios_date));
    return o.build(w.pretty, indent);
}

std::string to_json_virt(const W& w, const VirtualizationInfo& virt, int indent)
{
    JsonObj o;
    o.add("kind", w.en(virt.kind));
    o.add("hypervisor", w.str(virt.hypervisor));
    return o.build(w.pretty, indent);
}

std::string to_json_platform(const W& w, const PlatformInfo& p, int indent)
{
    JsonObj o;
    o.add("host", to_json_host(w, p.host, indent + 1));
    o.add("os", to_json_os(w, p.os, indent + 1));
    o.add("kernel", to_json_kernel(w, p.kernel, indent + 1));
    o.add("architecture", to_json_arch_info(w, p.architecture, indent + 1));
    if(p.firmware)
    {
        o.add("firmware", to_json_firmware(w, *p.firmware, indent + 1));
    }
    if(p.virtualization)
    {
        o.add("virtualization", to_json_virt(w, *p.virtualization, indent + 1));
    }
    return o.build(w.pretty, indent);
}

std::string to_json_cpu_package(const W& w, const CpuPackage& pkg, int indent)
{
    JsonObj o;
    o.add("id", w.id(pkg.id));
    o.add("vendor", w.named(pkg.vendor));
    o.add("model_name", w.named(pkg.model_name));
    o.add("physical_cores", w.u32(pkg.physical_cores));
    o.add("logical_threads", w.u32(pkg.logical_threads));
    if(pkg.base_frequency)
    {
        o.add("base_frequency", w.unit(*pkg.base_frequency));
    }
    if(pkg.max_frequency)
    {
        o.add("max_frequency", w.unit(*pkg.max_frequency));
    }
    return o.build(w.pretty, indent);
}

std::string to_json_cpu_core(const W& w, const CpuCore& core, int indent)
{
    JsonObj o;
    o.add("id", w.id(core.id));
    o.add("package_id", w.id(core.package_id));
    o.add("logical_threads", w.u32(core.logical_threads));
    if(core.numa_node)
    {
        o.add("numa_node", w.id(*core.numa_node));
    }
    return o.build(w.pretty, indent);
}

std::string to_json_logical_cpu(const W& w, const LogicalCpu& cpu, int indent)
{
    JsonObj o;
    o.add("id", w.id(cpu.id));
    o.add("core_id", w.id(cpu.core_id));
    o.add("package_id", w.id(cpu.package_id));
    if(cpu.numa_node)
    {
        o.add("numa_node", w.id(*cpu.numa_node));
    }
    o.add("visible_to_current_process", w.boolean(cpu.visible_to_current_process));
    return o.build(w.pretty, indent);
}

std::string to_json_numa_node(const W& w, const NumaNode& node, int indent)
{
    JsonObj o;
    o.add("id", w.id(node.id));
    if(node.local_memory)
    {
        o.add("local_memory", w.unit(*node.local_memory));
    }
    return o.build(w.pretty, indent);
}

std::string to_json_isa_ext(const W& w, const std::vector<IsaExtension>& exts, int indent)
{
    JsonArr a;
    for(auto e : exts)
    {
        a.add(w.en(e));
    }
    return a.build(w.pretty, indent);
}

std::string to_json_cpu(const W& w, const CpuSubsystem& cpu, int indent)
{
    JsonObj o;
    o.add("arch", w.en(cpu.arch));
    {
        JsonArr a;
        for(const auto& pkg : cpu.packages)
        {
            a.add(to_json_cpu_package(w, pkg, indent + 2));
        }
        o.add("packages", a.build(w.pretty, indent + 1));
    }
    {
        JsonArr a;
        for(const auto& core : cpu.cores)
        {
            a.add(to_json_cpu_core(w, core, indent + 2));
        }
        o.add("cores", a.build(w.pretty, indent + 1));
    }
    {
        JsonArr a;
        for(const auto& lc : cpu.logical_cpus)
        {
            a.add(to_json_logical_cpu(w, lc, indent + 2));
        }
        o.add("logical_cpus", a.build(w.pretty, indent + 1));
    }
    {
        JsonArr a;
        for(const auto& node : cpu.numa_nodes)
        {
            a.add(to_json_numa_node(w, node, indent + 2));
        }
        o.add("numa_nodes", a.build(w.pretty, indent + 1));
    }
    o.add("isa_extensions", to_json_isa_ext(w, cpu.isa_extensions, indent + 1));
    return o.build(w.pretty, indent);
}

std::string to_json_numa_memory(const W& w, const NumaMemoryInfo& info, int indent)
{
    JsonObj o;
    o.add("node", w.id(info.node));
    o.add("total", w.unit(info.total));
    if(info.available)
    {
        o.add("available", w.unit(*info.available));
    }
    return o.build(w.pretty, indent);
}

std::string to_json_memory(const W& w, const MemorySubsystem& mem, int indent)
{
    JsonObj o;
    o.add("total_memory", w.unit(mem.total_memory));
    if(mem.available_memory)
    {
        o.add("available_memory", w.unit(*mem.available_memory));
    }
    {
        JsonArr a;
        for(const auto& nm : mem.numa_memory)
        {
            a.add(to_json_numa_memory(w, nm, indent + 2));
        }
        o.add("numa_memory", a.build(w.pretty, indent + 1));
    }
    return o.build(w.pretty, indent);
}

std::string to_json_accelerator(const W& w, const AcceleratorDevice& dev, int indent)
{
    JsonObj o;
    o.add("id", w.id(dev.id));
    o.add("kind", w.en(dev.kind));
    o.add("vendor", w.named(dev.vendor));
    o.add("name", w.named(dev.name));
    o.add("pci_address", to_json_pci(w, dev.pci_address, indent + 1));
    if(dev.nearest_numa_node)
    {
        o.add("nearest_numa_node", w.id(*dev.nearest_numa_node));
    }
    if(dev.memory_size)
    {
        o.add("memory_size", w.unit(*dev.memory_size));
    }
    if(dev.driver)
    {
        o.add("driver", w.id(*dev.driver));
    }
    o.add("visible_to_current_process", w.boolean(dev.visible_to_current_process));
    return o.build(w.pretty, indent);
}

std::string to_json_accelerators(const W& w, const AcceleratorSubsystem& acc, int indent)
{
    JsonArr a;
    for(const auto& dev : acc.devices)
    {
        a.add(to_json_accelerator(w, dev, indent + 1));
    }
    return a.build(w.pretty, indent);
}

std::string to_json_ip_vec(const W& w, const std::vector<IpAddress>& addrs, int indent)
{
    JsonArr a;
    for(const auto& ip : addrs)
    {
        a.add(w.named(ip));
    }
    return a.build(w.pretty, indent);
}

std::string to_json_net_iface(const W& w, const NetworkInterface& iface, int indent)
{
    JsonObj o;
    o.add("name", w.named(iface.name));
    o.add("mac", w.named(iface.mac));
    o.add("state", w.en(iface.state));
    if(iface.speed)
    {
        o.add("speed", w.unit(*iface.speed));
    }
    o.add("addresses", to_json_ip_vec(w, iface.addresses, indent + 1));
    o.add("pci_address", to_json_pci(w, iface.pci_address, indent + 1));
    if(iface.rdma_device)
    {
        o.add("rdma_device", w.id(*iface.rdma_device));
    }
    o.add("visible_to_current_process", w.boolean(iface.visible_to_current_process));
    return o.build(w.pretty, indent);
}

std::string to_json_network(const W& w, const NetworkSubsystem& net, int indent)
{
    JsonArr a;
    for(const auto& iface : net.interfaces)
    {
        a.add(to_json_net_iface(w, iface, indent + 1));
    }
    return a.build(w.pretty, indent);
}

std::string to_json_pci_device(const W& w, const PciDevice& dev, int indent)
{
    JsonObj o;
    o.add("address", w.pci(dev.address, indent + 1));
    o.add("vendor", w.named(dev.vendor));
    o.add("device_name", w.named(dev.device_name));
    o.add("device_class", w.str(dev.device_class));
    if(dev.numa_node)
    {
        o.add("numa_node", w.id(*dev.numa_node));
    }
    return o.build(w.pretty, indent);
}

std::string to_json_pci_subsystem(const W& w, const PciSubsystem& pci, int indent)
{
    JsonArr a;
    for(const auto& dev : pci.devices)
    {
        a.add(to_json_pci_device(w, dev, indent + 1));
    }
    return a.build(w.pretty, indent);
}

std::string to_json_storage_device(const W& w, const StorageDevice& dev, int indent)
{
    JsonObj o;
    o.add("id", w.id(dev.id));
    o.add("name", w.named(dev.name));
    if(dev.capacity)
    {
        o.add("capacity", w.unit(*dev.capacity));
    }
    o.add("pci_address", to_json_pci(w, dev.pci_address, indent + 1));
    o.add("kind", w.en(dev.kind));
    return o.build(w.pretty, indent);
}

std::string to_json_storage(const W& w, const StorageSubsystem& storage, int indent)
{
    JsonArr a;
    for(const auto& dev : storage.devices)
    {
        a.add(to_json_storage_device(w, dev, indent + 1));
    }
    return a.build(w.pretty, indent);
}

std::string to_json_numa_relation(const W& w, const NumaRelation& rel, int indent)
{
    JsonObj o;
    o.add("node", w.id(rel.node));
    {
        JsonArr a;
        for(const auto& pid : rel.packages)
        {
            a.add(w.id(pid));
        }
        o.add("packages", a.build(w.pretty, indent + 1));
    }
    if(rel.local_memory)
    {
        o.add("local_memory", w.unit(*rel.local_memory));
    }
    return o.build(w.pretty, indent);
}

std::string to_json_pci_relation(const W& w, const PciRelation& rel, int indent)
{
    JsonObj o;
    o.add("parent", w.pci(rel.parent, indent + 1));
    o.add("child", w.pci(rel.child, indent + 1));
    return o.build(w.pretty, indent);
}

std::string to_json_device_locality(const W& w, const DeviceLocality& loc, int indent)
{
    JsonObj o;
    o.add("pci_address", w.pci(loc.pci_address, indent + 1));
    o.add("nearest_numa_node", w.id(loc.nearest_numa_node));
    return o.build(w.pretty, indent);
}

std::string to_json_topology(const W& w, const TopologyInfo& topo, int indent)
{
    JsonObj o;
    {
        JsonArr a;
        for(const auto& rel : topo.numa_relations)
        {
            a.add(to_json_numa_relation(w, rel, indent + 2));
        }
        o.add("numa_relations", a.build(w.pretty, indent + 1));
    }
    {
        JsonArr a;
        for(const auto& rel : topo.pci_relations)
        {
            a.add(to_json_pci_relation(w, rel, indent + 2));
        }
        o.add("pci_relations", a.build(w.pretty, indent + 1));
    }
    {
        JsonArr a;
        for(const auto& loc : topo.device_localities)
        {
            a.add(to_json_device_locality(w, loc, indent + 2));
        }
        o.add("device_localities", a.build(w.pretty, indent + 1));
    }
    return o.build(w.pretty, indent);
}

std::string to_json_resources(const W& w, const ResourceInfo& r, int indent)
{
    JsonObj o;
    o.add("cpu", to_json_cpu(w, r.cpu, indent + 1));
    o.add("memory", to_json_memory(w, r.memory, indent + 1));
    o.add("accelerators", to_json_accelerators(w, r.accelerators, indent + 1));
    o.add("network", to_json_network(w, r.network, indent + 1));
    o.add("storage", to_json_storage(w, r.storage, indent + 1));
    o.add("pci", to_json_pci_subsystem(w, r.pci, indent + 1));
    o.add("topology", to_json_topology(w, r.topology, indent + 1));
    return o.build(w.pretty, indent);
}

std::string to_json_driver(const W& w, const DriverInfo& d, int indent)
{
    JsonObj o;
    o.add("name", w.str(d.name));
    o.add("version", w.str(d.version));
    o.add("loaded", w.boolean(d.loaded));
    return o.build(w.pretty, indent);
}

std::string to_json_runtime(const W& w, const RuntimeInfo& r, int indent)
{
    JsonObj o;
    o.add("name", w.str(r.name));
    o.add("version", w.str(r.version));
    o.add("path", w.str(r.path));
    return o.build(w.pretty, indent);
}

std::string to_json_compiler(const W& w, const CompilerInfo& c, int indent)
{
    JsonObj o;
    o.add("name", w.str(c.name));
    o.add("version", w.str(c.version));
    o.add("path", w.str(c.path));
    return o.build(w.pretty, indent);
}

std::string to_json_library(const W& w, const LibraryInfo& l, int indent)
{
    JsonObj o;
    o.add("name", w.str(l.name));
    o.add("version", w.str(l.version));
    o.add("path", w.str(l.path));
    return o.build(w.pretty, indent);
}

std::string to_json_software(const W& w, const SoftwareStackInfo& s, int indent)
{
    JsonObj o;
    {
        JsonArr a;
        for(const auto& d : s.drivers)
        {
            a.add(to_json_driver(w, d, indent + 2));
        }
        o.add("drivers", a.build(w.pretty, indent + 1));
    }
    {
        JsonArr a;
        for(const auto& r : s.runtimes)
        {
            a.add(to_json_runtime(w, r, indent + 2));
        }
        o.add("runtimes", a.build(w.pretty, indent + 1));
    }
    {
        JsonArr a;
        for(const auto& c : s.compilers)
        {
            a.add(to_json_compiler(w, c, indent + 2));
        }
        o.add("compilers", a.build(w.pretty, indent + 1));
    }
    {
        JsonArr a;
        for(const auto& l : s.libraries)
        {
            a.add(to_json_library(w, l, indent + 2));
        }
        o.add("libraries", a.build(w.pretty, indent + 1));
    }
    if(s.cuda)
    {
        JsonObj c;
        c.add("driver_version", w.str(s.cuda->driver_version));
        c.add("runtime_version", w.str(s.cuda->runtime_version));
        c.add("device_count", w.u32(s.cuda->device_count));
        o.add("cuda", c.build(w.pretty, indent + 1));
    }
    if(s.rocm)
    {
        JsonObj r;
        r.add("version", w.str(s.rocm->version));
        o.add("rocm", r.build(w.pretty, indent + 1));
    }
    if(s.level_zero)
    {
        JsonObj l;
        l.add("version", w.str(s.level_zero->version));
        o.add("level_zero", l.build(w.pretty, indent + 1));
    }
    if(s.mpi)
    {
        JsonObj m;
        m.add("implementation", w.str(s.mpi->implementation));
        m.add("version", w.str(s.mpi->version));
        o.add("mpi", m.build(w.pretty, indent + 1));
    }
    if(s.rdma)
    {
        JsonObj r;
        r.add("ibverbs_available", w.boolean(s.rdma->ibverbs_available));
        r.add("rdma_devices", to_json_str_vec(w, s.rdma->rdma_devices, indent + 2));
        o.add("rdma", r.build(w.pretty, indent + 1));
    }
    return o.build(w.pretty, indent);
}

std::string to_json_process(const W& w, const ProcessInfo& p, int indent)
{
    JsonObj o;
    o.add("pid", std::to_string(p.pid));
    o.add("uid", std::to_string(p.uid));
    o.add("gid", std::to_string(p.gid));
    o.add("euid", std::to_string(p.euid));
    o.add("egid", std::to_string(p.egid));
    return o.build(w.pretty, indent);
}

std::string to_json_env_vars(const W& w, const EnvironmentInfo& env, int indent)
{
    JsonArr a;
    for(const auto& [k, v] : env.relevant_vars)
    {
        JsonObj o;
        o.add("key", w.str(k));
        o.add("value", w.str(v));
        a.add(o.build(w.pretty, indent + 1));
    }
    return a.build(w.pretty, indent);
}

std::string to_json_cgroup(const W& w, const CgroupInfo& cg, int indent)
{
    JsonObj o;
    o.add("version", w.en(cg.version));
    o.add("path", w.str(cg.path));
    return o.build(w.pretty, indent);
}

std::string to_json_cpuset(const W& w, const CpusetInfo& cs, int indent)
{
    JsonObj o;
    {
        JsonArr a;
        for(const auto& c : cs.cpus)
        {
            a.add(w.id(c));
        }
        o.add("cpus", a.build(w.pretty, indent + 1));
    }
    {
        JsonArr a;
        for(const auto& m : cs.mems)
        {
            a.add(w.id(m));
        }
        o.add("mems", a.build(w.pretty, indent + 1));
    }
    o.add("is_restricted", w.boolean(cs.is_restricted));
    return o.build(w.pretty, indent);
}

std::string to_json_permissions(const W& w, const PermissionInfo& perm, int indent)
{
    JsonObj o;
    o.add("is_root", w.boolean(perm.is_root));
    o.add("capabilities", to_json_str_vec(w, perm.capabilities, indent + 1));
    return o.build(w.pretty, indent);
}

std::string to_json_execution(const W& w, const ExecutionContextInfo& e, int indent)
{
    JsonObj o;
    o.add("process", to_json_process(w, e.process, indent + 1));
    o.add("environment", to_json_env_vars(w, e.environment, indent + 1));
    o.add("cgroup", to_json_cgroup(w, e.cgroup, indent + 1));
    o.add("cpuset", to_json_cpuset(w, e.cpuset, indent + 1));
    o.add("permissions", to_json_permissions(w, e.permissions, indent + 1));
    if(e.container)
    {
        JsonObj c;
        c.add("kind", w.en(e.container->kind));
        c.add("id", w.str(e.container->id));
        o.add("container", c.build(w.pretty, indent + 1));
    }
    {
        JsonArr a;
        for(const auto& id : e.visible_logical_cpu_ids)
        {
            a.add(w.id(id));
        }
        o.add("visible_logical_cpu_ids", a.build(w.pretty, indent + 1));
    }
    {
        JsonArr a;
        for(const auto& id : e.visible_accelerator_ids)
        {
            a.add(w.id(id));
        }
        o.add("visible_accelerator_ids", a.build(w.pretty, indent + 1));
    }
    {
        JsonArr a;
        for(const auto& name : e.visible_network_interface_names)
        {
            a.add(w.named(name));
        }
        o.add("visible_network_interface_names", a.build(w.pretty, indent + 1));
    }
    return o.build(w.pretty, indent);
}

std::string to_json_conflict(const W& w, const ConflictDetail& c, int indent)
{
    JsonObj o;
    o.add("field", w.str(c.field));
    o.add("value_from_higher", w.str(c.value_from_higher));
    o.add("value_from_lower", w.str(c.value_from_lower));
    o.add("higher_source", w.en(c.higher_source));
    o.add("lower_source", w.en(c.lower_source));
    return o.build(w.pretty, indent);
}

std::string to_json_diagnostic(const W& w, const Diagnostic& d, int indent)
{
    JsonObj o;
    o.add("severity", w.en(d.severity));
    o.add("message", w.str(d.message));
    if(d.source)
    {
        o.add("source", w.en(*d.source));
    }
    if(d.conflict)
    {
        o.add("conflict", to_json_conflict(w, *d.conflict, indent + 1));
    }
    return o.build(w.pretty, indent);
}

std::string to_json_diagnostics(const W& w, const Diagnostics& diag, int indent)
{
    JsonArr a;
    for(const auto& d : diag.records)
    {
        a.add(to_json_diagnostic(w, d, indent + 1));
    }
    return a.build(w.pretty, indent);
}

std::string to_json_raw(const W& w, const RawStore& raw, int /*indent*/)
{
    return detail::raw_store_to_json(raw, w.pretty);
}

} // namespace

std::string to_json(const SystemSnapshot& snapshot, const SerializationOptions& opts)
{
    const W w{opts.pretty_print};
    JsonObj root;
    if(opts.include_meta)
    {
        root.add("meta", to_json_meta(w, snapshot.meta, 1));
    }
    root.add("platform", to_json_platform(w, snapshot.platform, 1));
    root.add("resources", to_json_resources(w, snapshot.resources, 1));
    root.add("software", to_json_software(w, snapshot.software, 1));
    root.add("execution", to_json_execution(w, snapshot.execution, 1));
    root.add("diagnostics", to_json_diagnostics(w, snapshot.diagnostics, 1));
    if(opts.include_raw && snapshot.raw)
    {
        root.add("raw", to_json_raw(w, *snapshot.raw, 1));
    }
    return root.build(opts.pretty_print, 0);
}

Expected<SystemSnapshot, SysalError> from_json(std::string_view json)
{
    auto parsed = detail::parse_json(json);
    if(!parsed)
    {
        return make_unexpected(parsed.error());
    }
    const auto& root = *parsed;
    if(root.type != detail::JsonVal::Type::Obj)
    {
        return make_unexpected(
            SysalError(ErrorKind::DeserializationError, "root is not a JSON object"));
    }

    SystemSnapshot snapshot;

    const auto* meta = root.get("meta");
    if(meta != nullptr && meta->type == detail::JsonVal::Type::Obj)
    {
        const auto* version = meta->get("sysal_version");
        if(version != nullptr && version->as_str() != nullptr)
        {
            snapshot.meta.sysal_version = *version->as_str();
        }
        const auto* collect_time = meta->get("collect_time");
        if(collect_time != nullptr)
        {
            auto ms = collect_time->as_i64();
            if(ms)
            {
                snapshot.meta.collect_time = detail::ms_to_time_point(*ms);
            }
        }
        const auto* duration = meta->get("collect_duration_ms");
        if(duration != nullptr)
        {
            auto ms = duration->as_i64();
            if(ms)
            {
                snapshot.meta.collect_duration = std::chrono::milliseconds(*ms);
            }
        }
    }

    const auto* raw = root.get("raw");
    if(raw != nullptr && raw->type == detail::JsonVal::Type::Obj)
    {
        auto raw_store = detail::raw_store_from_json(*raw);
        if(raw_store)
        {
            snapshot.raw = *raw_store;
        }
    }

    return snapshot;
}

} // namespace sysal
