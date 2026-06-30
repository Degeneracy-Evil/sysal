/// @file serialization.cpp
/// @brief SystemSnapshot 的 JSON 序列化与反序列化实现
/// @details 通过匿名命名空间内的辅助结构 W 与一组 to_json_* 函数，将
///          SystemSnapshot 各子结构递归转换为 JSON 文本；from_json 仅还原
///          元数据与原始证据等少量字段，供回放场景使用。JSON 输出格式：
///          根对象包含 meta、platform、resources、software、execution、
///          diagnostics，以及可选的 raw 等键；数值以十进制字符串表示，
///          枚举以其底层整数值表示，布尔为 true/false，PCI 地址为含
///          domain/bus/device/function 四字段的对象。

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

/// @brief JSON 写入辅助结构
/// @details 封装 pretty 缩进开关与一组基础类型到 JSON 文本的转换方法，
///          供各 to_json_* 函数统一调用，避免重复的字符串转义与格式处理。
struct W
{
    bool pretty;

    /// @brief 将字符串转义为 JSON 字符串字面量
    /// @param s 原始字符串
    /// @return 已转义的 JSON 字符串（含引号）
    [[nodiscard]] std::string str(std::string_view s) const { return escape_string(s); }

    /// @brief 将枚举转为 JSON 数字（其底层整数值）
    /// @param e 枚举值
    /// @return 整数的十进制字符串
    template <typename E> [[nodiscard]] std::string en(E e) const
    {
        return std::to_string(static_cast<int>(e));
    }

    /// @brief 将 uint32 转为 JSON 数字字符串
    [[nodiscard]] std::string u32(std::uint32_t v) const { return std::to_string(v); }

    /// @brief 将 uint64 转为 JSON 数字字符串
    [[nodiscard]] std::string u64(std::uint64_t v) const { return std::to_string(v); }

    /// @brief 将 int64 转为 JSON 数字字符串
    [[nodiscard]] std::string i64(std::int64_t v) const { return std::to_string(v); }

    /// @brief 将 bool 转为 JSON 布尔字面量
    [[nodiscard]] std::string boolean(bool v) const { return v ? "true" : "false"; }

    /// @brief 将强类型 ID 转为 JSON 数字字符串
    /// @param v 强类型 ID（StrongId）值
    /// @return 内部 value 的十进制字符串
    template <typename T, typename Tag> [[nodiscard]] std::string id(StrongId<T, Tag> v) const
    {
        return std::to_string(v.value());
    }

    /// @brief 将命名字符串包装类型转为 JSON 字符串字面量
    template <typename Tag> [[nodiscard]] std::string named(NamedString<Tag> v) const
    {
        return escape_string(v.value);
    }

    /// @brief 将带单位的标量转为 JSON 数字字符串
    template <typename Tag> [[nodiscard]] std::string unit(ScalarUnit<Tag> v) const
    {
        return std::to_string(v.value);
    }

    /// @brief 将 PCI 地址转为 JSON 对象
    /// @details 输出形如 {"domain":..,"bus":..,"device":..,"function":..} 的对象。
    /// @param a PCI 地址
    /// @param indent 当前缩进层级
    /// @return PCI 地址的 JSON 对象文本
    [[nodiscard]] std::string pci(const PciAddress& a, int indent) const
    {
        JsonObj o;
        o.add("domain", std::to_string(a.domain));
        o.add("bus", std::to_string(a.bus));
        o.add("device", std::to_string(a.device));
        o.add("function", std::to_string(a.function));
        return o.build(pretty, indent);
    }

    /// @brief 将时间点转为自 epoch 起的毫秒数 JSON 字符串
    [[nodiscard]] std::string time_ms(std::chrono::system_clock::time_point tp) const
    {
        return time_point_to_ms(tp);
    }
};

/// @brief 将可选 PCI 地址序列化为 JSON
/// @details 有值时输出 PCI 地址对象，无值时输出 null。
/// @param w 写入辅助
/// @param addr 可选 PCI 地址
/// @param indent 当前缩进层级
/// @return JSON 文本
std::string to_json_pci(const W& w, const std::optional<PciAddress>& addr, int indent)
{
    if(addr)
    {
        return w.pci(*addr, indent);
    }
    return "null";
}

/// @brief 将采集规格序列化为 JSON 对象
/// @details 输出形如 {"raw":bool,"platform":bool,...,"execution_context":bool}
///          的对象，键名对应各采集开关。
/// @param w 写入辅助
/// @param spec 采集规格
/// @param indent 当前缩进层级
/// @return JSON 文本
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

/// @brief 将字符串向量序列化为 JSON 字符串数组
/// @details 输出形如 ["s1","s2",...] 的数组，每个元素经字符串转义。
/// @param w 写入辅助
/// @param vec 字符串列表
/// @param indent 当前缩进层级
/// @return JSON 文本
std::string to_json_str_vec(const W& w, const std::vector<std::string>& vec, int indent)
{
    JsonArr a;
    for(const auto& s : vec)
    {
        a.add(w.str(s));
    }
    return a.build(w.pretty, indent);
}

/// @brief 将快照元数据序列化为 JSON 对象
/// @details 输出包含 collect_time（epoch 毫秒）、sysal_version、
///          collect_duration_ms、requested_spec、succeeded_collectors、
///          failed_collectors 的对象。
/// @param w 写入辅助
/// @param meta 快照元数据
/// @param indent 当前缩进层级
/// @return JSON 文本
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

/// @brief 将主机信息序列化为 JSON 对象
/// @details 输出形如 {"hostname":"..."} 的对象。
std::string to_json_host(const W& w, const HostInfo& host, int indent)
{
    JsonObj o;
    o.add("hostname", w.str(host.hostname));
    return o.build(w.pretty, indent);
}

/// @brief 将操作系统信息序列化为 JSON 对象
/// @details 输出形如 {"name":"...","version":"..."} 的对象。
std::string to_json_os(const W& w, const OsInfo& os, int indent)
{
    JsonObj o;
    o.add("name", w.str(os.name));
    o.add("version", w.str(os.version));
    return o.build(w.pretty, indent);
}

/// @brief 将内核信息序列化为 JSON 对象
/// @details 输出形如 {"version":"...","release":"..."} 的对象。
std::string to_json_kernel(const W& w, const KernelInfo& kernel, int indent)
{
    JsonObj o;
    o.add("version", w.str(kernel.version));
    o.add("release", w.str(kernel.release));
    return o.build(w.pretty, indent);
}

/// @brief 将架构信息序列化为 JSON 对象
/// @details 输出形如 {"cpu_arch":<enum int>,"machine_arch":"..."} 的对象。
std::string to_json_arch_info(const W& w, const ArchitectureInfo& arch, int indent)
{
    JsonObj o;
    o.add("cpu_arch", w.en(arch.cpu_arch));
    o.add("machine_arch", w.str(arch.machine_arch));
    return o.build(w.pretty, indent);
}

/// @brief 将固件信息序列化为 JSON 对象
/// @details 输出形如 {"bios_version":"...","bios_vendor":"...","bios_date":"..."} 的对象。
std::string to_json_firmware(const W& w, const FirmwareInfo& fw, int indent)
{
    JsonObj o;
    o.add("bios_version", w.str(fw.bios_version));
    o.add("bios_vendor", w.str(fw.bios_vendor));
    o.add("bios_date", w.str(fw.bios_date));
    return o.build(w.pretty, indent);
}

/// @brief 将虚拟化信息序列化为 JSON 对象
/// @details 输出形如 {"kind":<enum int>,"hypervisor":"..."} 的对象。
std::string to_json_virt(const W& w, const VirtualizationInfo& virt, int indent)
{
    JsonObj o;
    o.add("kind", w.en(virt.kind));
    o.add("hypervisor", w.str(virt.hypervisor));
    return o.build(w.pretty, indent);
}

/// @brief 将平台信息序列化为 JSON 对象
/// @details 输出包含 host、os、kernel、architecture，以及可选 firmware、
///          virtualization 子对象的整体平台对象。
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

/// @brief 将 CPU 物理包序列化为 JSON 对象
/// @details 输出包含 id、vendor、model_name、physical_cores、logical_threads，
///          以及可选 base_frequency、max_frequency 的对象。
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

/// @brief 将 CPU 物理核序列化为 JSON 对象
/// @details 输出包含 id、package_id、logical_threads，以及可选 numa_node 的对象。
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

/// @brief 将逻辑 CPU 序列化为 JSON 对象
/// @details 输出包含 id、core_id、package_id，可选 numa_node，以及
///          visible_to_current_process 布尔字段的对象。
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

/// @brief 将 NUMA 节点序列化为 JSON 对象
/// @details 输出包含 id 与可选 local_memory 的对象。
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

/// @brief 将 ISA 扩展列表序列化为 JSON 数字数组
/// @details 输出形如 [<enum int>,...] 的数组。
std::string to_json_isa_ext(const W& w, const std::vector<IsaExtension>& exts, int indent)
{
    JsonArr a;
    for(auto e : exts)
    {
        a.add(w.en(e));
    }
    return a.build(w.pretty, indent);
}

/// @brief 将 CPU 子系统序列化为 JSON 对象
/// @details 输出包含 arch、packages 数组、cores 数组、logical_cpus 数组、
///          numa_nodes 数组，以及 isa_extensions 数组的对象。
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

/// @brief 将 NUMA 内存信息序列化为 JSON 对象
/// @details 输出包含 node、total，以及可选 available 的对象。
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

/// @brief 将内存子系统序列化为 JSON 对象
/// @details 输出包含 total_memory、可选 available_memory，以及 numa_memory
///          数组的对象。
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

/// @brief 将加速器设备序列化为 JSON 对象
/// @details 输出包含 id、kind、vendor、name、pci_address，可选 nearest_numa_node、
///          memory_size、driver，以及 visible_to_current_process 布尔字段的对象。
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

/// @brief 将加速器子系统序列化为 JSON 数组
/// @details 输出形如 [dev1,dev2,...] 的设备对象数组。
std::string to_json_accelerators(const W& w, const AcceleratorSubsystem& acc, int indent)
{
    JsonArr a;
    for(const auto& dev : acc.devices)
    {
        a.add(to_json_accelerator(w, dev, indent + 1));
    }
    return a.build(w.pretty, indent);
}

/// @brief 将 IP 地址列表序列化为 JSON 字符串数组
/// @details 输出形如 ["ip1","ip2",...] 的数组，每个地址经字符串转义。
std::string to_json_ip_vec(const W& w, const std::vector<IpAddress>& addrs, int indent)
{
    JsonArr a;
    for(const auto& ip : addrs)
    {
        a.add(w.named(ip));
    }
    return a.build(w.pretty, indent);
}

/// @brief 将网络接口序列化为 JSON 对象
/// @details 输出包含 name、mac、state，可选 speed，以及 addresses 数组、
///          pci_address，可选 rdma_device，与 visible_to_current_process 的对象。
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

/// @brief 将网络子系统序列化为 JSON 数组
/// @details 输出形如 [iface1,iface2,...] 的网络接口对象数组。
std::string to_json_network(const W& w, const NetworkSubsystem& net, int indent)
{
    JsonArr a;
    for(const auto& iface : net.interfaces)
    {
        a.add(to_json_net_iface(w, iface, indent + 1));
    }
    return a.build(w.pretty, indent);
}

/// @brief 将 PCI 设备序列化为 JSON 对象
/// @details 输出包含 address（PCI 地址对象）、vendor、device_name、device_class，
///          以及可选 numa_node 的对象。
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

/// @brief 将 PCI 子系统序列化为 JSON 数组
/// @details 输出形如 [dev1,dev2,...] 的 PCI 设备对象数组。
std::string to_json_pci_subsystem(const W& w, const PciSubsystem& pci, int indent)
{
    JsonArr a;
    for(const auto& dev : pci.devices)
    {
        a.add(to_json_pci_device(w, dev, indent + 1));
    }
    return a.build(w.pretty, indent);
}

/// @brief 将存储设备序列化为 JSON 对象
/// @details 输出包含 id、name，可选 capacity、pci_address，以及 kind 的对象。
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

/// @brief 将存储子系统序列化为 JSON 数组
/// @details 输出形如 [dev1,dev2,...] 的存储设备对象数组。
std::string to_json_storage(const W& w, const StorageSubsystem& storage, int indent)
{
    JsonArr a;
    for(const auto& dev : storage.devices)
    {
        a.add(to_json_storage_device(w, dev, indent + 1));
    }
    return a.build(w.pretty, indent);
}

/// @brief 将 NUMA 关系序列化为 JSON 对象
/// @details 输出包含 node、packages 数组，以及可选 local_memory 的对象。
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

/// @brief 将 PCI 父子关系序列化为 JSON 对象
/// @details 输出包含 parent 与 child 两个 PCI 地址子对象的关系对象。
std::string to_json_pci_relation(const W& w, const PciRelation& rel, int indent)
{
    JsonObj o;
    o.add("parent", w.pci(rel.parent, indent + 1));
    o.add("child", w.pci(rel.child, indent + 1));
    return o.build(w.pretty, indent);
}

/// @brief 将设备位置信息序列化为 JSON 对象
/// @details 输出包含 pci_address 与 nearest_numa_node 的对象。
std::string to_json_device_locality(const W& w, const DeviceLocality& loc, int indent)
{
    JsonObj o;
    o.add("pci_address", w.pci(loc.pci_address, indent + 1));
    o.add("nearest_numa_node", w.id(loc.nearest_numa_node));
    return o.build(w.pretty, indent);
}

/// @brief 将拓扑信息序列化为 JSON 对象
/// @details 输出包含 numa_relations 数组、pci_relations 数组，以及
///          device_localities 数组的对象。
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

/// @brief 将资源信息序列化为 JSON 对象
/// @details 输出包含 cpu、memory、accelerators、network、storage、pci、
///          topology 各子对象的整体资源对象。
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

/// @brief 将驱动信息序列化为 JSON 对象
/// @details 输出形如 {"name":"...","version":"...","loaded":bool} 的对象。
std::string to_json_driver(const W& w, const DriverInfo& d, int indent)
{
    JsonObj o;
    o.add("name", w.str(d.name));
    o.add("version", w.str(d.version));
    o.add("loaded", w.boolean(d.loaded));
    return o.build(w.pretty, indent);
}

/// @brief 将运行时信息序列化为 JSON 对象
/// @details 输出形如 {"name":"...","version":"...","path":"..."} 的对象。
std::string to_json_runtime(const W& w, const RuntimeInfo& r, int indent)
{
    JsonObj o;
    o.add("name", w.str(r.name));
    o.add("version", w.str(r.version));
    o.add("path", w.str(r.path));
    return o.build(w.pretty, indent);
}

/// @brief 将编译器信息序列化为 JSON 对象
/// @details 输出形如 {"name":"...","version":"...","path":"..."} 的对象。
std::string to_json_compiler(const W& w, const CompilerInfo& c, int indent)
{
    JsonObj o;
    o.add("name", w.str(c.name));
    o.add("version", w.str(c.version));
    o.add("path", w.str(c.path));
    return o.build(w.pretty, indent);
}

/// @brief 将库信息序列化为 JSON 对象
/// @details 输出形如 {"name":"...","version":"...","path":"..."} 的对象。
std::string to_json_library(const W& w, const LibraryInfo& l, int indent)
{
    JsonObj o;
    o.add("name", w.str(l.name));
    o.add("version", w.str(l.version));
    o.add("path", w.str(l.path));
    return o.build(w.pretty, indent);
}

/// @brief 将软件栈信息序列化为 JSON 对象
/// @details 输出包含 drivers、runtimes、compilers、libraries 四个数组，
///          以及可选 cuda、rocm、level_zero、mpi、rdma 子对象的整体软件栈对象。
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
    // CUDA 子对象仅当存在时输出
    if(s.cuda)
    {
        JsonObj c;
        c.add("driver_version", w.str(s.cuda->driver_version));
        c.add("runtime_version", w.str(s.cuda->runtime_version));
        c.add("device_count", w.u32(s.cuda->device_count));
        o.add("cuda", c.build(w.pretty, indent + 1));
    }
    // ROCm 子对象仅当存在时输出
    if(s.rocm)
    {
        JsonObj r;
        r.add("version", w.str(s.rocm->version));
        o.add("rocm", r.build(w.pretty, indent + 1));
    }
    // Level Zero 子对象仅当存在时输出
    if(s.level_zero)
    {
        JsonObj l;
        l.add("version", w.str(s.level_zero->version));
        o.add("level_zero", l.build(w.pretty, indent + 1));
    }
    // MPI 子对象仅当存在时输出
    if(s.mpi)
    {
        JsonObj m;
        m.add("implementation", w.str(s.mpi->implementation));
        m.add("version", w.str(s.mpi->version));
        o.add("mpi", m.build(w.pretty, indent + 1));
    }
    // RDMA 子对象仅当存在时输出
    if(s.rdma)
    {
        JsonObj r;
        r.add("ibverbs_available", w.boolean(s.rdma->ibverbs_available));
        r.add("rdma_devices", to_json_str_vec(w, s.rdma->rdma_devices, indent + 2));
        o.add("rdma", r.build(w.pretty, indent + 1));
    }
    return o.build(w.pretty, indent);
}

/// @brief 将进程信息序列化为 JSON 对象
/// @details 输出包含 pid、uid、gid、euid、egid 的对象。
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

/// @brief 将环境变量序列化为 JSON 对象数组
/// @details 输出形如 [{"key":"...","value":"..."},...] 的数组。
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

/// @brief 将 cgroup 信息序列化为 JSON 对象
/// @details 输出形如 {"version":<enum int>,"path":"..."} 的对象。
std::string to_json_cgroup(const W& w, const CgroupInfo& cg, int indent)
{
    JsonObj o;
    o.add("version", w.en(cg.version));
    o.add("path", w.str(cg.path));
    return o.build(w.pretty, indent);
}

/// @brief 将 cpuset 信息序列化为 JSON 对象
/// @details 输出包含 cpus 数组、mems 数组，以及 is_restricted 布尔字段的对象。
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

/// @brief 将权限信息序列化为 JSON 对象
/// @details 输出包含 is_root 布尔字段与 capabilities 字符串数组的对象。
std::string to_json_permissions(const W& w, const PermissionInfo& perm, int indent)
{
    JsonObj o;
    o.add("is_root", w.boolean(perm.is_root));
    o.add("capabilities", to_json_str_vec(w, perm.capabilities, indent + 1));
    return o.build(w.pretty, indent);
}

/// @brief 将执行上下文序列化为 JSON 对象
/// @details 输出包含 process、environment、cgroup、cpuset、permissions，
///          可选 container 子对象，以及 visible_logical_cpu_ids、
///          visible_accelerator_ids、visible_network_interface_names 三个数组的对象。
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

/// @brief 将冲突详情序列化为 JSON 对象
/// @details 输出包含 field、value_from_higher、value_from_lower、
///          higher_source、lower_source 的对象。
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

/// @brief 将单条诊断序列化为 JSON 对象
/// @details 输出包含 severity、message，可选 source 与 conflict 子对象的对象。
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

/// @brief 将诊断集合序列化为 JSON 数组
/// @details 输出形如 [diag1,diag2,...] 的诊断对象数组。
std::string to_json_diagnostics(const W& w, const Diagnostics& diag, int indent)
{
    JsonArr a;
    for(const auto& d : diag.records)
    {
        a.add(to_json_diagnostic(w, d, indent + 1));
    }
    return a.build(w.pretty, indent);
}

/// @brief 将原始证据存储序列化为 JSON
/// @details 委托给 detail::raw_store_to_json，按其内部格式输出原始记录对象。
/// @param w 写入辅助（仅用其 pretty 开关）
/// @param raw 原始证据存储
/// @param indent 当前缩进层级（未使用，由 raw_store_to_json 自行处理）
/// @return JSON 文本
std::string to_json_raw(const W& w, const RawStore& raw, int /*indent*/)
{
    return detail::raw_store_to_json(raw, w.pretty);
}

} // namespace

/// @brief 将 SystemSnapshot 序列化为 JSON 字符串
/// @details 依据 opts 决定是否美化输出与是否包含 meta、raw；根对象依次包含
///          meta（可选）、platform、resources、software、execution、diagnostics，
///          以及可选 raw。输出格式为单一 JSON 对象。
/// @param snapshot 待序列化的系统快照
/// @param opts 序列化选项（美化、是否包含元数据、是否包含原始证据）
/// @return JSON 文本
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

/// @brief 从 JSON 字符串反序列化为 SystemSnapshot
/// @details 当前为部分反序列化：仅还原 meta 中的 sysal_version、collect_time、
///          collect_duration，以及可选的 raw 子对象；其余字段保持默认值。
///          根节点非对象时返回 DeserializationError。
/// @param json JSON 文本
/// @return 成功返回 SystemSnapshot，失败返回 SysalError
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

    // 还原 meta 子对象中的版本、采集时间与耗时
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

    // 还原可选的 raw 子对象
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
