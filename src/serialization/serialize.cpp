/// @file serialize.cpp
/// @brief JSON 序列化与反序列化实现
/// @details 实现 RawStore ↔ JSON 与 System ↔ JSON 的转换，以及基于文件的
///          save/load 操作。System 序列化输出顶层对象含 info、meta、warnings、
///          raw 四个字段。使用 nlohmann/json 库进行 JSON 处理。

#include <nlohmann/json.hpp>

#include "sysal/core/error.hpp"
#include "sysal/model/raw_store.hpp"
#include "sysal/serialization/serialization.hpp"
#include "sysal/test/replay.hpp"
#include "sysal/types/enums.hpp"
#include "sysal/version.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sysal
{
namespace
{

using json = nlohmann::json;

// ───────────────────────────── 辅助工具 ─────────────────────────────

/// @brief 将时间点转换为 epoch 毫秒
/// @param tp 系统时钟时间点
/// @return epoch 毫秒数
[[nodiscard]] std::int64_t time_point_to_ms(std::chrono::system_clock::time_point tp)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

/// @brief 将 epoch 毫秒数转换为系统时钟时间点
/// @param ms epoch 毫秒数
/// @return 对应的 time_point
[[nodiscard]] std::chrono::system_clock::time_point ms_to_time_point(std::int64_t ms)
{
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
}

/// @brief 从 JSON 数组读取字符串列表
/// @param arr JSON 数组
/// @return 字符串向量
[[nodiscard]] std::vector<std::string> str_array_from_json(const json& arr)
{
    std::vector<std::string> result;
    for(const auto& elem : arr)
    {
        result.push_back(elem.get<std::string>());
    }
    return result;
}

// ───────────────────────────── PciAddress ─────────────────────────────

[[nodiscard]] json pci_address_to_json(const PciAddress& addr)
{
    return json{
        {"domain", static_cast<unsigned>(addr.domain)},
        {"bus", static_cast<unsigned>(addr.bus)},
        {"device", static_cast<unsigned>(addr.device)},
        {"function", static_cast<unsigned>(addr.function)},
    };
}

[[nodiscard]] PciAddress pci_address_from_json(const json& j)
{
    PciAddress addr;
    addr.domain = static_cast<std::uint16_t>(j.at("domain").get<unsigned>());
    addr.bus = static_cast<std::uint8_t>(j.at("bus").get<unsigned>());
    addr.device = static_cast<std::uint8_t>(j.at("device").get<unsigned>());
    addr.function = static_cast<std::uint8_t>(j.at("function").get<unsigned>());
    return addr;
}

// ───────────────────────────── RawRecord / RawStore ─────────────────────────────

[[nodiscard]] json raw_record_to_json(const RawRecord& rec)
{
    return json{
        {"source", static_cast<std::uint64_t>(rec.source)},
        {"path_or_command", rec.path_or_command},
        {"payload", rec.payload},
        {"status", static_cast<std::uint64_t>(rec.status)},
        {"collected_at", time_point_to_ms(rec.collected_at)},
    };
}

[[nodiscard]] RawRecord raw_record_from_json(const json& j)
{
    RawRecord rec;
    rec.source = static_cast<RawSource>(j.at("source").get<std::uint64_t>());
    j.at("path_or_command").get_to(rec.path_or_command);
    j.at("payload").get_to(rec.payload);
    rec.status = static_cast<CollectStatus>(j.at("status").get<std::uint64_t>());
    rec.collected_at = ms_to_time_point(j.at("collected_at").get<std::int64_t>());
    return rec;
}

[[nodiscard]] json raw_store_to_json(const RawStore& store)
{
    json arr = json::array();
    for(const auto& rec : store.records)
    {
        arr.push_back(raw_record_to_json(rec));
    }
    return json{{"records", std::move(arr)}};
}

[[nodiscard]] RawStore raw_store_from_json(const json& root)
{
    RawStore store;
    for(const auto& elem : root.at("records"))
    {
        store.records.push_back(raw_record_from_json(elem));
    }
    return store;
}

// ───────────────────────────── Platform ─────────────────────────────

[[nodiscard]] json host_to_json(const Host& h)
{
    return json{
        {"hostname", h.hostname},   {"machine_id", h.machine_id}, {"product_name", h.product_name},
        {"vendor", h.vendor.value}, {"serial", h.serial},
    };
}

[[nodiscard]] Host host_from_json(const json& j)
{
    Host h;
    j.at("hostname").get_to(h.hostname);
    j.at("machine_id").get_to(h.machine_id);
    j.at("product_name").get_to(h.product_name);
    j.at("vendor").get_to(h.vendor.value);
    j.at("serial").get_to(h.serial);
    return h;
}

[[nodiscard]] json os_to_json(const Os& o)
{
    return json{
        {"name", o.name},
        {"version", o.version},
        {"distribution", o.distribution},
        {"distribution_version", o.distribution_version},
        {"codename", o.codename},
    };
}

[[nodiscard]] Os os_from_json(const json& j)
{
    Os o;
    j.at("name").get_to(o.name);
    j.at("version").get_to(o.version);
    j.at("distribution").get_to(o.distribution);
    j.at("distribution_version").get_to(o.distribution_version);
    j.at("codename").get_to(o.codename);
    return o;
}

[[nodiscard]] json kernel_to_json(const Kernel& k)
{
    return json{
        {"release", k.release},
        {"version", k.version},
        {"compiled_at", k.compiled_at},
        {"architecture", k.architecture},
    };
}

[[nodiscard]] Kernel kernel_from_json(const json& j)
{
    Kernel k;
    j.at("release").get_to(k.release);
    j.at("version").get_to(k.version);
    j.at("compiled_at").get_to(k.compiled_at);
    j.at("architecture").get_to(k.architecture);
    return k;
}

[[nodiscard]] json arch_to_json(const Architecture& a)
{
    return json{
        {"name", a.name},
        {"bits", a.bits},
        {"byte_order", a.byte_order},
    };
}

[[nodiscard]] Architecture arch_from_json(const json& j)
{
    Architecture a;
    j.at("name").get_to(a.name);
    a.bits = j.at("bits").get<std::uint32_t>();
    j.at("byte_order").get_to(a.byte_order);
    return a;
}

[[nodiscard]] json firmware_to_json(const Firmware& f)
{
    return json{
        {"bios_vendor", f.bios_vendor},
        {"bios_version", f.bios_version},
        {"bios_date", f.bios_date},
        {"uefi", f.uefi},
    };
}

[[nodiscard]] Firmware firmware_from_json(const json& j)
{
    Firmware f;
    j.at("bios_vendor").get_to(f.bios_vendor);
    j.at("bios_version").get_to(f.bios_version);
    j.at("bios_date").get_to(f.bios_date);
    f.uefi = j.at("uefi").get<bool>();
    return f;
}

[[nodiscard]] json virt_to_json(const Virtualization& v)
{
    return json{
        {"kind", static_cast<std::uint32_t>(v.kind)},
        {"hypervisor", v.hypervisor},
        {"container", v.container},
    };
}

[[nodiscard]] Virtualization virt_from_json(const json& j)
{
    Virtualization v;
    v.kind = static_cast<VirtualizationKind>(j.at("kind").get<std::uint32_t>());
    j.at("hypervisor").get_to(v.hypervisor);
    v.container = j.at("container").get<bool>();
    return v;
}

[[nodiscard]] json platform_to_json(const Platform& p)
{
    json j = {
        {"host", host_to_json(p.host)},
        {"os", os_to_json(p.os)},
        {"kernel", kernel_to_json(p.kernel)},
        {"architecture", arch_to_json(p.architecture)},
    };
    if(p.firmware)
    {
        j["firmware"] = firmware_to_json(*p.firmware);
    }
    if(p.virtualization)
    {
        j["virtualization"] = virt_to_json(*p.virtualization);
    }
    return j;
}

[[nodiscard]] Platform platform_from_json(const json& j)
{
    Platform p;
    p.host = host_from_json(j.at("host"));
    p.os = os_from_json(j.at("os"));
    p.kernel = kernel_from_json(j.at("kernel"));
    p.architecture = arch_from_json(j.at("architecture"));
    if(j.contains("firmware"))
    {
        p.firmware = firmware_from_json(j.at("firmware"));
    }
    if(j.contains("virtualization"))
    {
        p.virtualization = virt_from_json(j.at("virtualization"));
    }
    return p;
}

// ───────────────────────────── Cpu ─────────────────────────────

[[nodiscard]] json cpu_package_to_json(const CpuPackage& pkg)
{
    json j = {
        {"id", pkg.id.value()},
        {"vendor", pkg.vendor.value},
        {"model_name", pkg.model_name.value},
        {"physical_cores", pkg.physical_cores},
        {"logical_threads", pkg.logical_threads},
    };
    if(pkg.base_frequency)
    {
        j["base_frequency"] = pkg.base_frequency->value;
    }
    if(pkg.max_frequency)
    {
        j["max_frequency"] = pkg.max_frequency->value;
    }
    return j;
}

[[nodiscard]] CpuPackage cpu_package_from_json(const json& j)
{
    CpuPackage pkg;
    pkg.id = CpuPackageId(j.at("id").get<std::uint32_t>());
    j.at("vendor").get_to(pkg.vendor.value);
    j.at("model_name").get_to(pkg.model_name.value);
    pkg.physical_cores = j.at("physical_cores").get<std::uint32_t>();
    pkg.logical_threads = j.at("logical_threads").get<std::uint32_t>();
    if(j.contains("base_frequency"))
    {
        pkg.base_frequency = Frequency{j.at("base_frequency").get<std::uint64_t>()};
    }
    if(j.contains("max_frequency"))
    {
        pkg.max_frequency = Frequency{j.at("max_frequency").get<std::uint64_t>()};
    }
    return pkg;
}

[[nodiscard]] json cpu_core_to_json(const CpuCore& c)
{
    json j = {
        {"id", c.id.value()},
        {"package_id", c.package_id.value()},
        {"logical_threads", c.logical_threads},
    };
    if(c.numa_node)
    {
        j["numa_node"] = c.numa_node->value();
    }
    return j;
}

[[nodiscard]] CpuCore cpu_core_from_json(const json& j)
{
    CpuCore c;
    c.id = CpuCoreId(j.at("id").get<std::uint32_t>());
    c.package_id = CpuPackageId(j.at("package_id").get<std::uint32_t>());
    c.logical_threads = j.at("logical_threads").get<std::uint32_t>();
    if(j.contains("numa_node"))
    {
        c.numa_node = NumaNodeId(j.at("numa_node").get<std::uint32_t>());
    }
    return c;
}

[[nodiscard]] json logical_cpu_to_json(const LogicalCpu& lc)
{
    json j = {
        {"id", lc.id.value()},
        {"core_id", lc.core_id.value()},
        {"package_id", lc.package_id.value()},
        {"visible_to_current_process", lc.visible_to_current_process},
    };
    if(lc.numa_node)
    {
        j["numa_node"] = lc.numa_node->value();
    }
    return j;
}

[[nodiscard]] LogicalCpu logical_cpu_from_json(const json& j)
{
    LogicalCpu lc;
    lc.id = LogicalCpuId(j.at("id").get<std::uint32_t>());
    lc.core_id = CpuCoreId(j.at("core_id").get<std::uint32_t>());
    lc.package_id = CpuPackageId(j.at("package_id").get<std::uint32_t>());
    if(j.contains("numa_node"))
    {
        lc.numa_node = NumaNodeId(j.at("numa_node").get<std::uint32_t>());
    }
    lc.visible_to_current_process = j.at("visible_to_current_process").get<bool>();
    return lc;
}

[[nodiscard]] json numa_node_to_json(const NumaNode& n)
{
    json cpus = json::array();
    for(const auto& cpu_id : n.cpus)
    {
        cpus.push_back(cpu_id.value());
    }
    return json{{"id", n.id.value()}, {"cpus", std::move(cpus)}};
}

[[nodiscard]] NumaNode numa_node_from_json(const json& j)
{
    NumaNode n;
    n.id = NumaNodeId(j.at("id").get<std::uint32_t>());
    for(const auto& elem : j.at("cpus"))
    {
        n.cpus.push_back(LogicalCpuId(elem.get<std::uint32_t>()));
    }
    return n;
}

[[nodiscard]] json cpu_to_json(const Cpu& c)
{
    json packages = json::array();
    for(const auto& pkg : c.packages)
    {
        packages.push_back(cpu_package_to_json(pkg));
    }
    json cores = json::array();
    for(const auto& core : c.cores)
    {
        cores.push_back(cpu_core_to_json(core));
    }
    json logical_cpus = json::array();
    for(const auto& lc : c.logical_cpus)
    {
        logical_cpus.push_back(logical_cpu_to_json(lc));
    }
    json numa_nodes = json::array();
    for(const auto& n : c.numa_nodes)
    {
        numa_nodes.push_back(numa_node_to_json(n));
    }
    json isa = json::array();
    for(const auto& ext : c.isa_extensions)
    {
        isa.push_back(static_cast<std::uint32_t>(ext));
    }
    return json{
        {"arch", static_cast<std::uint32_t>(c.arch)},
        {"packages", std::move(packages)},
        {"cores", std::move(cores)},
        {"logical_cpus", std::move(logical_cpus)},
        {"numa_nodes", std::move(numa_nodes)},
        {"isa_extensions", std::move(isa)},
    };
}

[[nodiscard]] Cpu cpu_from_json(const json& j)
{
    Cpu c;
    c.arch = static_cast<Arch>(j.at("arch").get<std::uint32_t>());
    for(const auto& elem : j.at("packages"))
    {
        c.packages.push_back(cpu_package_from_json(elem));
    }
    for(const auto& elem : j.at("cores"))
    {
        c.cores.push_back(cpu_core_from_json(elem));
    }
    for(const auto& elem : j.at("logical_cpus"))
    {
        c.logical_cpus.push_back(logical_cpu_from_json(elem));
    }
    for(const auto& elem : j.at("numa_nodes"))
    {
        c.numa_nodes.push_back(numa_node_from_json(elem));
    }
    for(const auto& elem : j.at("isa_extensions"))
    {
        c.isa_extensions.push_back(static_cast<IsaExtension>(elem.get<std::uint32_t>()));
    }
    return c;
}

// ───────────────────────────── Memory ─────────────────────────────

[[nodiscard]] json numa_memory_to_json(const NumaMemory& nm)
{
    json j = {
        {"node", nm.node.value()},
        {"total", nm.total.value},
    };
    if(nm.available)
    {
        j["available"] = nm.available->value;
    }
    return j;
}

[[nodiscard]] NumaMemory numa_memory_from_json(const json& j)
{
    NumaMemory nm;
    nm.node = NumaNodeId(j.at("node").get<std::uint32_t>());
    nm.total = MemorySize{j.at("total").get<std::uint64_t>()};
    if(j.contains("available"))
    {
        nm.available = MemorySize{j.at("available").get<std::uint64_t>()};
    }
    return nm;
}

[[nodiscard]] json memory_to_json(const Memory& m)
{
    json j = {{"total_memory", m.total_memory.value}};
    if(m.available_memory)
    {
        j["available_memory"] = m.available_memory->value;
    }
    json arr = json::array();
    for(const auto& nm : m.numa_memory)
    {
        arr.push_back(numa_memory_to_json(nm));
    }
    j["numa_memory"] = std::move(arr);
    return j;
}

[[nodiscard]] Memory memory_from_json(const json& j)
{
    Memory m;
    m.total_memory = MemorySize{j.at("total_memory").get<std::uint64_t>()};
    if(j.contains("available_memory"))
    {
        m.available_memory = MemorySize{j.at("available_memory").get<std::uint64_t>()};
    }
    if(j.contains("numa_memory"))
    {
        for(const auto& elem : j.at("numa_memory"))
        {
            m.numa_memory.push_back(numa_memory_from_json(elem));
        }
    }
    return m;
}

// ───────────────────────────── Accelerator ─────────────────────────────

[[nodiscard]] json accel_device_to_json(const AcceleratorDevice& d)
{
    json j = {
        {"id", d.id.value()},
        {"kind", static_cast<std::uint32_t>(d.kind)},
        {"vendor", d.vendor.value},
        {"name", d.name.value},
        {"visible_to_current_process", d.visible_to_current_process},
    };
    if(d.pci_address)
    {
        j["pci_address"] = pci_address_to_json(*d.pci_address);
    }
    if(d.nearest_numa_node)
    {
        j["nearest_numa_node"] = d.nearest_numa_node->value();
    }
    if(d.memory_size)
    {
        j["memory_size"] = d.memory_size->value;
    }
    if(d.driver)
    {
        j["driver"] = d.driver->value();
    }
    return j;
}

[[nodiscard]] AcceleratorDevice accel_device_from_json(const json& j)
{
    AcceleratorDevice d;
    d.id = AcceleratorId(j.at("id").get<std::uint32_t>());
    d.kind = static_cast<AcceleratorKind>(j.at("kind").get<std::uint32_t>());
    j.at("vendor").get_to(d.vendor.value);
    j.at("name").get_to(d.name.value);
    if(j.contains("pci_address"))
    {
        d.pci_address = pci_address_from_json(j.at("pci_address"));
    }
    if(j.contains("nearest_numa_node"))
    {
        d.nearest_numa_node = NumaNodeId(j.at("nearest_numa_node").get<std::uint32_t>());
    }
    if(j.contains("memory_size"))
    {
        d.memory_size = MemorySize{j.at("memory_size").get<std::uint64_t>()};
    }
    if(j.contains("driver"))
    {
        d.driver = DriverId(j.at("driver").get<std::uint32_t>());
    }
    d.visible_to_current_process = j.at("visible_to_current_process").get<bool>();
    return d;
}

[[nodiscard]] json accelerators_to_json(const Accelerators& a)
{
    json arr = json::array();
    for(const auto& dev : a.devices)
    {
        arr.push_back(accel_device_to_json(dev));
    }
    return json{{"devices", std::move(arr)}};
}

[[nodiscard]] Accelerators accelerators_from_json(const json& j)
{
    Accelerators a;
    for(const auto& elem : j.at("devices"))
    {
        a.devices.push_back(accel_device_from_json(elem));
    }
    return a;
}

// ───────────────────────────── Network ─────────────────────────────

[[nodiscard]] json net_iface_to_json(const NetworkInterface& ni)
{
    json j = {
        {"name", ni.name.value},
        {"mac", ni.mac.value},
        {"state", static_cast<std::uint32_t>(ni.state)},
        {"visible_to_current_process", ni.visible_to_current_process},
    };
    if(ni.speed)
    {
        j["speed"] = ni.speed->value;
    }
    json addrs = json::array();
    for(const auto& addr : ni.addresses)
    {
        addrs.push_back(addr.value);
    }
    j["addresses"] = std::move(addrs);
    if(ni.pci_address)
    {
        j["pci_address"] = pci_address_to_json(*ni.pci_address);
    }
    return j;
}

[[nodiscard]] NetworkInterface net_iface_from_json(const json& j)
{
    NetworkInterface ni;
    j.at("name").get_to(ni.name.value);
    j.at("mac").get_to(ni.mac.value);
    ni.state = static_cast<InterfaceState>(j.at("state").get<std::uint32_t>());
    if(j.contains("speed"))
    {
        ni.speed = Bandwidth{j.at("speed").get<std::uint64_t>()};
    }
    if(j.contains("addresses"))
    {
        for(const auto& elem : j.at("addresses"))
        {
            IpAddress ip;
            ip.value = elem.get<std::string>();
            ni.addresses.push_back(std::move(ip));
        }
    }
    if(j.contains("pci_address"))
    {
        ni.pci_address = pci_address_from_json(j.at("pci_address"));
    }
    ni.visible_to_current_process = j.at("visible_to_current_process").get<bool>();
    return ni;
}

[[nodiscard]] json network_to_json(const Network& n)
{
    json arr = json::array();
    for(const auto& iface : n.interfaces)
    {
        arr.push_back(net_iface_to_json(iface));
    }
    return json{{"interfaces", std::move(arr)}};
}

[[nodiscard]] Network network_from_json(const json& j)
{
    Network n;
    for(const auto& elem : j.at("interfaces"))
    {
        n.interfaces.push_back(net_iface_from_json(elem));
    }
    return n;
}

// ───────────────────────────── Storage ─────────────────────────────

[[nodiscard]] json storage_dev_to_json(const StorageDevice& sd)
{
    json j = {
        {"id", sd.id.value()},
        {"name", sd.name.value},
        {"kind", static_cast<std::uint32_t>(sd.kind)},
    };
    if(sd.capacity)
    {
        j["capacity"] = sd.capacity->value;
    }
    if(sd.pci_address)
    {
        j["pci_address"] = pci_address_to_json(*sd.pci_address);
    }
    return j;
}

[[nodiscard]] StorageDevice storage_dev_from_json(const json& j)
{
    StorageDevice sd;
    sd.id = StorageId(j.at("id").get<std::uint32_t>());
    j.at("name").get_to(sd.name.value);
    if(j.contains("capacity"))
    {
        sd.capacity = MemorySize{j.at("capacity").get<std::uint64_t>()};
    }
    if(j.contains("pci_address"))
    {
        sd.pci_address = pci_address_from_json(j.at("pci_address"));
    }
    sd.kind = static_cast<StorageKind>(j.at("kind").get<std::uint32_t>());
    return sd;
}

[[nodiscard]] json storage_to_json(const Storage& s)
{
    json arr = json::array();
    for(const auto& dev : s.devices)
    {
        arr.push_back(storage_dev_to_json(dev));
    }
    return json{{"devices", std::move(arr)}};
}

[[nodiscard]] Storage storage_from_json(const json& j)
{
    Storage s;
    for(const auto& elem : j.at("devices"))
    {
        s.devices.push_back(storage_dev_from_json(elem));
    }
    return s;
}

// ───────────────────────────── Pci ─────────────────────────────

[[nodiscard]] json pci_device_to_json(const PciDevice& pd)
{
    json j = {
        {"address", pci_address_to_json(pd.address)},
        {"vendor", pd.vendor.value},
        {"device_name", pd.device_name.value},
        {"device_class", pd.device_class.value},
    };
    if(pd.numa_node)
    {
        j["numa_node"] = pd.numa_node->value();
    }
    return j;
}

[[nodiscard]] PciDevice pci_device_from_json(const json& j)
{
    PciDevice pd;
    pd.address = pci_address_from_json(j.at("address"));
    j.at("vendor").get_to(pd.vendor.value);
    j.at("device_name").get_to(pd.device_name.value);
    j.at("device_class").get_to(pd.device_class.value);
    if(j.contains("numa_node"))
    {
        pd.numa_node = NumaNodeId(j.at("numa_node").get<std::uint32_t>());
    }
    return pd;
}

[[nodiscard]] json pci_to_json(const Pci& p)
{
    json arr = json::array();
    for(const auto& dev : p.devices)
    {
        arr.push_back(pci_device_to_json(dev));
    }
    return json{{"devices", std::move(arr)}};
}

[[nodiscard]] Pci pci_from_json(const json& j)
{
    Pci p;
    for(const auto& elem : j.at("devices"))
    {
        p.devices.push_back(pci_device_from_json(elem));
    }
    return p;
}

// ───────────────────────────── Software ─────────────────────────────

[[nodiscard]] json driver_to_json(const Driver& d)
{
    return json{
        {"id", d.id.value()}, {"name", d.name}, {"version", d.version},
        {"loaded", d.loaded}, {"path", d.path},
    };
}

[[nodiscard]] Driver driver_from_json(const json& j)
{
    Driver d;
    d.id = DriverId(j.at("id").get<std::uint32_t>());
    j.at("name").get_to(d.name);
    j.at("version").get_to(d.version);
    d.loaded = j.at("loaded").get<bool>();
    j.at("path").get_to(d.path);
    return d;
}

[[nodiscard]] json runtime_to_json(const Runtime& r)
{
    return json{
        {"name", r.name},
        {"version", r.version},
        {"path", r.path},
        {"env_var", r.env_var},
    };
}

[[nodiscard]] Runtime runtime_from_json(const json& j)
{
    Runtime r;
    j.at("name").get_to(r.name);
    j.at("version").get_to(r.version);
    j.at("path").get_to(r.path);
    j.at("env_var").get_to(r.env_var);
    return r;
}

[[nodiscard]] json compiler_to_json(const Compiler& c)
{
    return json{
        {"name", c.name},
        {"version", c.version},
        {"path", c.path},
        {"target", c.target},
    };
}

[[nodiscard]] Compiler compiler_from_json(const json& j)
{
    Compiler c;
    j.at("name").get_to(c.name);
    j.at("version").get_to(c.version);
    j.at("path").get_to(c.path);
    j.at("target").get_to(c.target);
    return c;
}

[[nodiscard]] json library_to_json(const Library& l)
{
    return json{
        {"name", l.name},
        {"version", l.version},
        {"path", l.path},
        {"kind", l.kind},
    };
}

[[nodiscard]] Library library_from_json(const json& j)
{
    Library l;
    j.at("name").get_to(l.name);
    j.at("version").get_to(l.version);
    j.at("path").get_to(l.path);
    j.at("kind").get_to(l.kind);
    return l;
}

[[nodiscard]] json cuda_to_json(const Cuda& c)
{
    return json{
        {"version", c.version},
        {"driver_version", c.driver_version},
        {"nvcc_path", c.nvcc_path},
        {"home", c.home},
    };
}

[[nodiscard]] Cuda cuda_from_json(const json& j)
{
    Cuda c;
    j.at("version").get_to(c.version);
    j.at("driver_version").get_to(c.driver_version);
    j.at("nvcc_path").get_to(c.nvcc_path);
    j.at("home").get_to(c.home);
    return c;
}

[[nodiscard]] json rocm_to_json(const Rocm& r)
{
    return json{
        {"version", r.version},
        {"hip_path", r.hip_path},
        {"rocm_path", r.rocm_path},
    };
}

[[nodiscard]] Rocm rocm_from_json(const json& j)
{
    Rocm r;
    j.at("version").get_to(r.version);
    j.at("hip_path").get_to(r.hip_path);
    j.at("rocm_path").get_to(r.rocm_path);
    return r;
}

[[nodiscard]] json level_zero_to_json(const LevelZero& lz)
{
    return json{
        {"version", lz.version},
        {"loader_path", lz.loader_path},
    };
}

[[nodiscard]] LevelZero level_zero_from_json(const json& j)
{
    LevelZero lz;
    j.at("version").get_to(lz.version);
    j.at("loader_path").get_to(lz.loader_path);
    return lz;
}

[[nodiscard]] json mpi_to_json(const Mpi& m)
{
    return json{
        {"implementation", m.implementation},
        {"version", m.version},
        {"path", m.path},
    };
}

[[nodiscard]] Mpi mpi_from_json(const json& j)
{
    Mpi m;
    j.at("implementation").get_to(m.implementation);
    j.at("version").get_to(m.version);
    j.at("path").get_to(m.path);
    return m;
}

[[nodiscard]] json rdma_to_json(const RdmaStack& r)
{
    return json{
        {"rdma_core_version", r.rdma_core_version},
        {"ibverbs_path", r.ibverbs_path},
        {"ucx_version", r.ucx_version},
    };
}

[[nodiscard]] RdmaStack rdma_from_json(const json& j)
{
    RdmaStack r;
    j.at("rdma_core_version").get_to(r.rdma_core_version);
    j.at("ibverbs_path").get_to(r.ibverbs_path);
    j.at("ucx_version").get_to(r.ucx_version);
    return r;
}

[[nodiscard]] json software_to_json(const SoftwareStack& sw)
{
    json j;

    json drivers = json::array();
    for(const auto& d : sw.drivers)
    {
        drivers.push_back(driver_to_json(d));
    }
    j["drivers"] = std::move(drivers);

    json runtimes = json::array();
    for(const auto& r : sw.runtimes)
    {
        runtimes.push_back(runtime_to_json(r));
    }
    j["runtimes"] = std::move(runtimes);

    json compilers = json::array();
    for(const auto& c : sw.compilers)
    {
        compilers.push_back(compiler_to_json(c));
    }
    j["compilers"] = std::move(compilers);

    json libraries = json::array();
    for(const auto& l : sw.libraries)
    {
        libraries.push_back(library_to_json(l));
    }
    j["libraries"] = std::move(libraries);

    if(sw.cuda)
    {
        j["cuda"] = cuda_to_json(*sw.cuda);
    }
    if(sw.rocm)
    {
        j["rocm"] = rocm_to_json(*sw.rocm);
    }
    if(sw.level_zero)
    {
        j["level_zero"] = level_zero_to_json(*sw.level_zero);
    }
    if(sw.mpi)
    {
        j["mpi"] = mpi_to_json(*sw.mpi);
    }
    if(sw.rdma)
    {
        j["rdma"] = rdma_to_json(*sw.rdma);
    }

    return j;
}

[[nodiscard]] SoftwareStack software_from_json(const json& j)
{
    SoftwareStack sw;

    for(const auto& elem : j.at("drivers"))
    {
        sw.drivers.push_back(driver_from_json(elem));
    }
    for(const auto& elem : j.at("runtimes"))
    {
        sw.runtimes.push_back(runtime_from_json(elem));
    }
    for(const auto& elem : j.at("compilers"))
    {
        sw.compilers.push_back(compiler_from_json(elem));
    }
    for(const auto& elem : j.at("libraries"))
    {
        sw.libraries.push_back(library_from_json(elem));
    }

    if(j.contains("cuda"))
    {
        sw.cuda = cuda_from_json(j.at("cuda"));
    }
    if(j.contains("rocm"))
    {
        sw.rocm = rocm_from_json(j.at("rocm"));
    }
    if(j.contains("level_zero"))
    {
        sw.level_zero = level_zero_from_json(j.at("level_zero"));
    }
    if(j.contains("mpi"))
    {
        sw.mpi = mpi_from_json(j.at("mpi"));
    }
    if(j.contains("rdma"))
    {
        sw.rdma = rdma_from_json(j.at("rdma"));
    }

    return sw;
}

// ───────────────────────────── Execution ─────────────────────────────

[[nodiscard]] json process_to_json(const Process& p)
{
    return json{
        {"pid", p.pid},   {"ppid", p.ppid}, {"uid", p.uid}, {"gid", p.gid},
        {"comm", p.comm}, {"exe", p.exe},   {"cwd", p.cwd},
    };
}

[[nodiscard]] Process process_from_json(const json& j)
{
    Process p;
    p.pid = j.at("pid").get<std::int32_t>();
    p.ppid = j.at("ppid").get<std::int32_t>();
    p.uid = j.at("uid").get<std::uint32_t>();
    p.gid = j.at("gid").get<std::uint32_t>();
    j.at("comm").get_to(p.comm);
    j.at("exe").get_to(p.exe);
    j.at("cwd").get_to(p.cwd);
    return p;
}

[[nodiscard]] json environment_to_json(const Environment& e)
{
    json arr = json::array();
    for(const auto& [k, v] : e.entries)
    {
        arr.push_back(json{{"key", k}, {"value", v}});
    }
    return json{{"entries", std::move(arr)}};
}

[[nodiscard]] Environment environment_from_json(const json& j)
{
    Environment e;
    for(const auto& elem : j.at("entries"))
    {
        e.entries.emplace_back(elem.at("key").get<std::string>(),
                               elem.at("value").get<std::string>());
    }
    return e;
}

[[nodiscard]] json cgroup_to_json(const Cgroup& c)
{
    json j = {
        {"version", static_cast<std::uint32_t>(c.version)},
        {"path", c.path},
    };
    json arr = json::array();
    for(const auto& ctrl : c.controllers)
    {
        arr.push_back(ctrl);
    }
    j["controllers"] = std::move(arr);
    return j;
}

[[nodiscard]] Cgroup cgroup_from_json(const json& j)
{
    Cgroup c;
    c.version = static_cast<CgroupVersion>(j.at("version").get<std::uint32_t>());
    j.at("path").get_to(c.path);
    if(j.contains("controllers"))
    {
        c.controllers = str_array_from_json(j.at("controllers"));
    }
    return c;
}

[[nodiscard]] json cpuset_to_json(const Cpuset& cs)
{
    return json{
        {"cpus", cs.cpus},
        {"mems", cs.mems},
        {"cpus_effective", cs.cpus_effective},
        {"mems_effective", cs.mems_effective},
    };
}

[[nodiscard]] Cpuset cpuset_from_json(const json& j)
{
    Cpuset cs;
    j.at("cpus").get_to(cs.cpus);
    j.at("mems").get_to(cs.mems);
    j.at("cpus_effective").get_to(cs.cpus_effective);
    j.at("mems_effective").get_to(cs.mems_effective);
    return cs;
}

[[nodiscard]] json permission_to_json(const Permission& p)
{
    json j = {
        {"euid", p.euid},
        {"egid", p.egid},
        {"is_root", p.is_root},
    };
    json arr = json::array();
    for(const auto& cap : p.capabilities)
    {
        arr.push_back(cap);
    }
    j["capabilities"] = std::move(arr);
    return j;
}

[[nodiscard]] Permission permission_from_json(const json& j)
{
    Permission p;
    p.euid = j.at("euid").get<std::uint32_t>();
    p.egid = j.at("egid").get<std::uint32_t>();
    if(j.contains("capabilities"))
    {
        p.capabilities = str_array_from_json(j.at("capabilities"));
    }
    p.is_root = j.at("is_root").get<bool>();
    return p;
}

[[nodiscard]] json container_to_json(const Container& c)
{
    return json{
        {"kind", static_cast<std::uint32_t>(c.kind)},
        {"id", c.id},
        {"runtime", c.runtime},
    };
}

[[nodiscard]] Container container_from_json(const json& j)
{
    Container c;
    c.kind = static_cast<ContainerKind>(j.at("kind").get<std::uint32_t>());
    j.at("id").get_to(c.id);
    j.at("runtime").get_to(c.runtime);
    return c;
}

[[nodiscard]] json execution_to_json(const ExecutionContext& e)
{
    json j = {
        {"process", process_to_json(e.process)},
        {"environment", environment_to_json(e.environment)},
        {"cgroup", cgroup_to_json(e.cgroup)},
        {"cpuset", cpuset_to_json(e.cpuset)},
        {"permission", permission_to_json(e.permission)},
    };
    if(e.container)
    {
        j["container"] = container_to_json(*e.container);
    }

    json vcpu = json::array();
    for(const auto& id : e.visible_logical_cpu_ids)
    {
        vcpu.push_back(id.value());
    }
    j["visible_logical_cpu_ids"] = std::move(vcpu);

    json vacc = json::array();
    for(const auto& id : e.visible_accelerator_ids)
    {
        vacc.push_back(id.value());
    }
    j["visible_accelerator_ids"] = std::move(vacc);

    json vnet = json::array();
    for(const auto& name : e.visible_network_interface_names)
    {
        vnet.push_back(name.value);
    }
    j["visible_network_interface_names"] = std::move(vnet);

    return j;
}

[[nodiscard]] ExecutionContext execution_from_json(const json& j)
{
    ExecutionContext e;
    e.process = process_from_json(j.at("process"));
    e.environment = environment_from_json(j.at("environment"));
    e.cgroup = cgroup_from_json(j.at("cgroup"));
    e.cpuset = cpuset_from_json(j.at("cpuset"));
    e.permission = permission_from_json(j.at("permission"));

    if(j.contains("container"))
    {
        e.container = container_from_json(j.at("container"));
    }

    for(const auto& elem : j.at("visible_logical_cpu_ids"))
    {
        e.visible_logical_cpu_ids.push_back(LogicalCpuId(elem.get<std::uint32_t>()));
    }
    for(const auto& elem : j.at("visible_accelerator_ids"))
    {
        e.visible_accelerator_ids.push_back(AcceleratorId(elem.get<std::uint32_t>()));
    }
    for(const auto& elem : j.at("visible_network_interface_names"))
    {
        InterfaceName in;
        in.value = elem.get<std::string>();
        e.visible_network_interface_names.push_back(std::move(in));
    }

    return e;
}

// ───────────────────────────── SystemInfo ─────────────────────────────

[[nodiscard]] json system_info_to_json(const SystemInfo& info)
{
    return json{
        {"platform", platform_to_json(info.platform)},
        {"cpu", cpu_to_json(info.cpu)},
        {"memory", memory_to_json(info.memory)},
        {"accelerators", accelerators_to_json(info.accelerators)},
        {"network", network_to_json(info.network)},
        {"storage", storage_to_json(info.storage)},
        {"pci", pci_to_json(info.pci)},
        {"software", software_to_json(info.software)},
        {"execution", execution_to_json(info.execution)},
    };
}

[[nodiscard]] SystemInfo system_info_from_json(const json& j)
{
    SystemInfo info;
    info.platform = platform_from_json(j.at("platform"));
    info.cpu = cpu_from_json(j.at("cpu"));
    info.memory = memory_from_json(j.at("memory"));
    info.accelerators = accelerators_from_json(j.at("accelerators"));
    info.network = network_from_json(j.at("network"));
    info.storage = storage_from_json(j.at("storage"));
    info.pci = pci_from_json(j.at("pci"));
    info.software = software_from_json(j.at("software"));
    info.execution = execution_from_json(j.at("execution"));
    return info;
}

// ───────────────────────────── SnapshotMeta ─────────────────────────────

[[nodiscard]] json meta_to_json(const SnapshotMeta& m)
{
    json j = {
        {"collect_time", time_point_to_ms(m.collect_time)},
        {"sysal_version", m.sysal_version},
        {"collect_duration", m.collect_duration.count()},
        {"requested_flags", static_cast<std::uint32_t>(m.requested_flags)},
    };

    json succ = json::array();
    for(const auto& s : m.succeeded_collectors)
    {
        succ.push_back(s);
    }
    j["succeeded_collectors"] = std::move(succ);

    json fail = json::array();
    for(const auto& f : m.failed_collectors)
    {
        fail.push_back(f);
    }
    j["failed_collectors"] = std::move(fail);

    return j;
}

[[nodiscard]] SnapshotMeta meta_from_json(const json& j)
{
    SnapshotMeta m;
    m.collect_time = ms_to_time_point(j.at("collect_time").get<std::int64_t>());
    j.at("sysal_version").get_to(m.sysal_version);
    m.collect_duration = std::chrono::duration<double>(j.at("collect_duration").get<double>());
    m.requested_flags = static_cast<Collect>(j.at("requested_flags").get<std::uint32_t>());
    m.succeeded_collectors = str_array_from_json(j.at("succeeded_collectors"));
    m.failed_collectors = str_array_from_json(j.at("failed_collectors"));
    return m;
}

} // namespace

// ══════════════════════════════════════════════════════════════════════════
// 公共接口：sysal::test
// ══════════════════════════════════════════════════════════════════════════

namespace test
{

void save_raw_store(const RawStore& raw, const std::string& path)
{
    std::ofstream ofs(path);
    if(!ofs)
    {
        throw SysalError(ErrorKind::IoError, "cannot open file for writing: " + path);
    }
    ofs << raw_store_to_json(raw).dump(4);
    if(!ofs)
    {
        throw SysalError(ErrorKind::IoError, "write failed: " + path);
    }
}

RawStore load_raw_store(const std::string& path)
{
    std::ifstream ifs(path);
    if(!ifs)
    {
        throw SysalError(ErrorKind::FileNotFound, "cannot open file for reading: " + path);
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    if(!ifs && !ifs.eof())
    {
        throw SysalError(ErrorKind::IoError, "read failed: " + path);
    }

    try
    {
        auto root = json::parse(oss.str());
        if(!root.is_object())
        {
            throw SysalError(ErrorKind::DeserializationError,
                             "RawStore JSON root must be an object");
        }
        return raw_store_from_json(root);
    }
    catch(const json::exception& e)
    {
        throw SysalError(ErrorKind::DeserializationError, e.what());
    }
}

} // namespace test

// ══════════════════════════════════════════════════════════════════════════
// 公共接口：sysal
// ══════════════════════════════════════════════════════════════════════════

std::string to_json(const System& sys, const SerializationOptions& opts)
{
    json root;
    root["info"] = system_info_to_json(sys.info);

    if(opts.include_meta)
    {
        root["meta"] = meta_to_json(sys.meta);
    }

    json warnings = json::array();
    for(const auto& w : sys.warnings)
    {
        warnings.push_back(w);
    }
    root["warnings"] = std::move(warnings);

    if(opts.include_raw && sys.raw)
    {
        root["raw"] = raw_store_to_json(*sys.raw);
    }

    return root.dump(opts.pretty_print ? 4 : -1);
}

System from_json(std::string_view json_str)
{
    try
    {
        auto root = json::parse(json_str);
        if(!root.is_object())
        {
            throw SysalError(ErrorKind::DeserializationError, "JSON root must be an object");
        }

        // 版本兼容性检查
        if(root.contains("meta") && root.at("meta").is_object())
        {
            const auto& meta = root.at("meta");
            if(meta.contains("sysal_version") && meta.at("sysal_version").is_string())
            {
                auto ver = meta.at("sysal_version").get<std::string>();
                auto prefix =
                    std::to_string(VERSION_MAJOR) + "." + std::to_string(VERSION_MINOR) + ".";
                if(!ver.starts_with(prefix))
                {
                    throw SysalError(ErrorKind::DeserializationError,
                                     "incompatible version: " + ver);
                }
            }
        }

        System sys;
        sys.info = system_info_from_json(root.at("info"));

        if(root.contains("meta") && root.at("meta").is_object())
        {
            sys.meta = meta_from_json(root.at("meta"));
        }

        if(root.contains("warnings") && root.at("warnings").is_array())
        {
            sys.warnings = str_array_from_json(root.at("warnings"));
        }

        if(root.contains("raw") && root.at("raw").is_object())
        {
            sys.raw = raw_store_from_json(root.at("raw"));
        }

        return sys;
    }
    catch(const json::exception& e)
    {
        throw SysalError(ErrorKind::DeserializationError, e.what());
    }
}

} // namespace sysal
