/// @file serialize.cpp
/// @brief JSON 序列化与反序列化实现
/// @details 实现 RawStore ↔ JSON 与 System ↔ JSON 的转换，以及基于文件的
///          save/load 操作。System 序列化输出顶层对象含 info、meta、warnings、
///          raw 四个字段。

#include "serialization/json.hpp"

#include "sysal/core/error.hpp"
#include "sysal/model/raw_store.hpp"
#include "sysal/serialization/serialization.hpp"
#include "sysal/test/replay.hpp"
#include "sysal/types/enums.hpp"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

namespace sysal::test
{

namespace
{

// ───────────────────────────── RawStore 序列化 ─────────────────────────────

/// @brief 将 RawRecord 序列化为 JSON 对象文本
/// @param rec 原始记录
/// @return JSON 对象字符串
[[nodiscard]] std::string raw_record_to_json(const RawRecord& rec)
{
    detail::JsonObj obj;
    obj.add("source", std::to_string(static_cast<std::uint64_t>(rec.source)));
    obj.add("path_or_command", detail::escape_string(rec.path_or_command));
    obj.add("payload", detail::escape_string(rec.payload));
    obj.add("status", std::to_string(static_cast<std::uint64_t>(rec.status)));
    obj.add("collected_at", detail::time_point_to_ms(rec.collected_at));
    return obj.build(true, 2);
}

/// @brief 将 RawStore 序列化为 JSON 文本
/// @param store 原始证据存储
/// @return JSON 文本字符串
[[nodiscard]] std::string raw_store_to_json(const RawStore& store)
{
    detail::JsonArr arr;
    for(const auto& rec : store.records)
    {
        arr.add(raw_record_to_json(rec));
    }
    detail::JsonObj root;
    root.add("records", arr.build(true, 1));
    return root.build(true, 0);
}

/// @brief 从 JsonVal 解析 RawRecord
/// @param obj JSON 对象值
/// @return 解析后的 RawRecord
/// @throws SysalError 字段缺失或类型错误时抛出
[[nodiscard]] RawRecord raw_record_from_json(const detail::JsonVal& obj)
{
    RawRecord rec;

    // source
    const auto* source_val = obj.get("source");
    if(!source_val)
    {
        throw SysalError(ErrorKind::DeserializationError, "missing 'source' field in record");
    }
    auto source_int = source_val->as_u64();
    if(!source_int)
    {
        throw SysalError(ErrorKind::DeserializationError, "'source' must be an integer");
    }
    rec.source = static_cast<RawSource>(*source_int);

    // path_or_command
    const auto* path_val = obj.get("path_or_command");
    if(!path_val)
    {
        throw SysalError(ErrorKind::DeserializationError,
                         "missing 'path_or_command' field in record");
    }
    const auto* path_str = path_val->as_str();
    if(!path_str)
    {
        throw SysalError(ErrorKind::DeserializationError, "'path_or_command' must be a string");
    }
    rec.path_or_command = *path_str;

    // payload
    const auto* payload_val = obj.get("payload");
    if(!payload_val)
    {
        throw SysalError(ErrorKind::DeserializationError, "missing 'payload' field in record");
    }
    const auto* payload_str = payload_val->as_str();
    if(!payload_str)
    {
        throw SysalError(ErrorKind::DeserializationError, "'payload' must be a string");
    }
    rec.payload = *payload_str;

    // status
    const auto* status_val = obj.get("status");
    if(!status_val)
    {
        throw SysalError(ErrorKind::DeserializationError, "missing 'status' field in record");
    }
    auto status_int = status_val->as_u64();
    if(!status_int)
    {
        throw SysalError(ErrorKind::DeserializationError, "'status' must be an integer");
    }
    rec.status = static_cast<CollectStatus>(*status_int);

    // collected_at
    const auto* time_val = obj.get("collected_at");
    if(!time_val)
    {
        throw SysalError(ErrorKind::DeserializationError, "missing 'collected_at' field in record");
    }
    auto time_int = time_val->as_i64();
    if(!time_int)
    {
        throw SysalError(ErrorKind::DeserializationError, "'collected_at' must be an integer");
    }
    rec.collected_at = detail::ms_to_time_point(*time_int);

    return rec;
}

/// @brief 从 JsonVal 解析 RawStore
/// @param root JSON 根对象
/// @return 解析后的 RawStore
/// @throws SysalError 结构错误时抛出
[[nodiscard]] RawStore raw_store_from_json(const detail::JsonVal& root)
{
    const auto* records_val = root.get("records");
    if(!records_val || records_val->type != detail::JsonVal::Type::Arr)
    {
        throw SysalError(ErrorKind::DeserializationError,
                         "missing or invalid 'records' array in RawStore JSON");
    }

    RawStore store;
    for(const auto& elem : records_val->arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError,
                             "each element in 'records' must be a JSON object");
        }
        store.records.push_back(raw_record_from_json(elem));
    }
    return store;
}

} // namespace

void save_raw_store(const RawStore& raw, const std::string& path)
{
    std::ofstream ofs(path);
    if(!ofs)
    {
        throw SysalError(ErrorKind::IoError, "cannot open file for writing: " + path);
    }
    ofs << raw_store_to_json(raw);
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
        auto root = detail::parse_json(oss.str());
        if(root.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError,
                             "RawStore JSON root must be an object");
        }
        return raw_store_from_json(root);
    }
    catch(const detail::JsonError& e)
    {
        throw SysalError(ErrorKind::DeserializationError, e.what());
    }
}

} // namespace sysal::test

// ══════════════════════════════════════════════════════════════════════════
// System ↔ JSON 序列化（namespace sysal）
// ══════════════════════════════════════════════════════════════════════════

namespace sysal
{

namespace
{

// ──────────────────────────── 辅助：JSON 值构建 ────────────────────────────

[[nodiscard]] std::string json_str(std::string_view s) { return detail::escape_string(s); }

[[nodiscard]] std::string json_u64(std::uint64_t v) { return std::to_string(v); }

[[nodiscard]] std::string json_u32(std::uint32_t v) { return std::to_string(v); }

[[nodiscard]] std::string json_i32(std::int32_t v) { return std::to_string(v); }

[[nodiscard]] std::string json_bool(bool v) { return v ? "true" : "false"; }

[[nodiscard]] std::string json_double(double v)
{
    // 使用 snprintf 保证精度
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6f", v);
    return buf;
}

// ──────────────────────── 辅助：JSON 值读取 ────────────────────────────────

/// @brief 从 JsonVal 读取字符串字段，缺失时抛出
[[nodiscard]] std::string req_str(const detail::JsonVal& obj, std::string_view key)
{
    const auto* v = obj.get(key);
    if(!v)
    {
        throw SysalError(ErrorKind::DeserializationError, "missing field: " + std::string(key));
    }
    const auto* s = v->as_str();
    if(!s)
    {
        throw SysalError(ErrorKind::DeserializationError,
                         "field '" + std::string(key) + "' must be a string");
    }
    return *s;
}

/// @brief 从 JsonVal 读取 uint64 字段，缺失时抛出
[[nodiscard]] std::uint64_t req_u64(const detail::JsonVal& obj, std::string_view key)
{
    const auto* v = obj.get(key);
    if(!v)
    {
        throw SysalError(ErrorKind::DeserializationError, "missing field: " + std::string(key));
    }
    auto n = v->as_u64();
    if(!n)
    {
        throw SysalError(ErrorKind::DeserializationError,
                         "field '" + std::string(key) + "' must be an integer");
    }
    return *n;
}

/// @brief 从 JsonVal 读取 int64 字段，缺失时抛出
[[nodiscard]] std::int64_t req_i64(const detail::JsonVal& obj, std::string_view key)
{
    const auto* v = obj.get(key);
    if(!v)
    {
        throw SysalError(ErrorKind::DeserializationError, "missing field: " + std::string(key));
    }
    auto n = v->as_i64();
    if(!n)
    {
        throw SysalError(ErrorKind::DeserializationError,
                         "field '" + std::string(key) + "' must be an integer");
    }
    return *n;
}

/// @brief 从 JsonVal 读取 bool 字段，缺失时抛出
[[nodiscard]] bool req_bool(const detail::JsonVal& obj, std::string_view key)
{
    const auto* v = obj.get(key);
    if(!v)
    {
        throw SysalError(ErrorKind::DeserializationError, "missing field: " + std::string(key));
    }
    auto b = v->as_bool();
    if(!b)
    {
        throw SysalError(ErrorKind::DeserializationError,
                         "field '" + std::string(key) + "' must be a boolean");
    }
    return *b;
}

/// @brief 从 JsonVal 读取 double 字段（从 Num str_val 解析），缺失时抛出
[[nodiscard]] double req_double(const detail::JsonVal& obj, std::string_view key)
{
    const auto* v = obj.get(key);
    if(!v)
    {
        throw SysalError(ErrorKind::DeserializationError, "missing field: " + std::string(key));
    }
    if(v->type != detail::JsonVal::Type::Num)
    {
        throw SysalError(ErrorKind::DeserializationError,
                         "field '" + std::string(key) + "' must be a number");
    }
    // 用 strtod 解析（from_chars 不支持 double 在部分实现中）
    char* end = nullptr;
    double result = std::strtod(v->str_val.c_str(), &end);
    if(end == v->str_val.c_str())
    {
        throw SysalError(ErrorKind::DeserializationError,
                         "field '" + std::string(key) + "' is not a valid number");
    }
    return result;
}

/// @brief 从 JsonVal 读取可选 uint64 字段
[[nodiscard]] std::optional<std::uint64_t> opt_u64(const detail::JsonVal& obj, std::string_view key)
{
    const auto* v = obj.get(key);
    if(!v)
    {
        return std::nullopt;
    }
    return v->as_u64();
}

/// @brief 获取必需的子对象
[[nodiscard]] const detail::JsonVal& req_obj(const detail::JsonVal& parent, std::string_view key)
{
    const auto* v = parent.get(key);
    if(!v || v->type != detail::JsonVal::Type::Obj)
    {
        throw SysalError(ErrorKind::DeserializationError,
                         "missing or invalid object field: " + std::string(key));
    }
    return *v;
}

/// @brief 获取必需的子数组
[[nodiscard]] const detail::JsonVal& req_arr(const detail::JsonVal& parent, std::string_view key)
{
    const auto* v = parent.get(key);
    if(!v || v->type != detail::JsonVal::Type::Arr)
    {
        throw SysalError(ErrorKind::DeserializationError,
                         "missing or invalid array field: " + std::string(key));
    }
    return *v;
}

/// @brief 从 JSON 数组读取字符串列表
[[nodiscard]] std::vector<std::string> str_array_from_json(const detail::JsonVal& arr)
{
    std::vector<std::string> result;
    for(const auto& elem : arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Str)
        {
            throw SysalError(ErrorKind::DeserializationError, "expected string in array");
        }
        result.push_back(elem.str_val);
    }
    return result;
}

// ──────────────────────── PciAddress 序列化 ────────────────────────────────

[[nodiscard]] std::string pci_address_to_json(const PciAddress& addr, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("domain", json_u64(addr.domain));
    obj.add("bus", json_u64(addr.bus));
    obj.add("device", json_u64(addr.device));
    obj.add("function", json_u64(addr.function));
    return obj.build(pretty, indent);
}

[[nodiscard]] PciAddress pci_address_from_json(const detail::JsonVal& obj)
{
    PciAddress addr;
    addr.domain = static_cast<std::uint16_t>(req_u64(obj, "domain"));
    addr.bus = static_cast<std::uint8_t>(req_u64(obj, "bus"));
    addr.device = static_cast<std::uint8_t>(req_u64(obj, "device"));
    addr.function = static_cast<std::uint8_t>(req_u64(obj, "function"));
    return addr;
}

// ──────────────────────── Platform 序列化 ──────────────────────────────────

[[nodiscard]] std::string host_to_json(const Host& h, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("hostname", json_str(h.hostname));
    obj.add("machine_id", json_str(h.machine_id));
    obj.add("product_name", json_str(h.product_name));
    obj.add("vendor", json_str(h.vendor.value));
    obj.add("serial", json_str(h.serial));
    return obj.build(pretty, indent);
}

[[nodiscard]] Host host_from_json(const detail::JsonVal& obj)
{
    Host h;
    h.hostname = req_str(obj, "hostname");
    h.machine_id = req_str(obj, "machine_id");
    h.product_name = req_str(obj, "product_name");
    h.vendor.value = req_str(obj, "vendor");
    h.serial = req_str(obj, "serial");
    return h;
}

[[nodiscard]] std::string os_to_json(const Os& o, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("name", json_str(o.name));
    obj.add("version", json_str(o.version));
    obj.add("distribution", json_str(o.distribution));
    obj.add("distribution_version", json_str(o.distribution_version));
    obj.add("codename", json_str(o.codename));
    return obj.build(pretty, indent);
}

[[nodiscard]] Os os_from_json(const detail::JsonVal& obj)
{
    Os o;
    o.name = req_str(obj, "name");
    o.version = req_str(obj, "version");
    o.distribution = req_str(obj, "distribution");
    o.distribution_version = req_str(obj, "distribution_version");
    o.codename = req_str(obj, "codename");
    return o;
}

[[nodiscard]] std::string kernel_to_json(const Kernel& k, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("release", json_str(k.release));
    obj.add("version", json_str(k.version));
    obj.add("compiled_at", json_str(k.compiled_at));
    obj.add("architecture", json_str(k.architecture));
    return obj.build(pretty, indent);
}

[[nodiscard]] Kernel kernel_from_json(const detail::JsonVal& obj)
{
    Kernel k;
    k.release = req_str(obj, "release");
    k.version = req_str(obj, "version");
    k.compiled_at = req_str(obj, "compiled_at");
    k.architecture = req_str(obj, "architecture");
    return k;
}

[[nodiscard]] std::string arch_to_json(const Architecture& a, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("name", json_str(a.name));
    obj.add("bits", json_u32(a.bits));
    obj.add("byte_order", json_str(a.byte_order));
    return obj.build(pretty, indent);
}

[[nodiscard]] Architecture arch_from_json(const detail::JsonVal& obj)
{
    Architecture a;
    a.name = req_str(obj, "name");
    a.bits = static_cast<std::uint32_t>(req_u64(obj, "bits"));
    a.byte_order = req_str(obj, "byte_order");
    return a;
}

[[nodiscard]] std::string firmware_to_json(const Firmware& f, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("bios_vendor", json_str(f.bios_vendor));
    obj.add("bios_version", json_str(f.bios_version));
    obj.add("bios_date", json_str(f.bios_date));
    obj.add("uefi", json_bool(f.uefi));
    return obj.build(pretty, indent);
}

[[nodiscard]] Firmware firmware_from_json(const detail::JsonVal& obj)
{
    Firmware f;
    f.bios_vendor = req_str(obj, "bios_vendor");
    f.bios_version = req_str(obj, "bios_version");
    f.bios_date = req_str(obj, "bios_date");
    f.uefi = req_bool(obj, "uefi");
    return f;
}

[[nodiscard]] std::string virt_to_json(const Virtualization& v, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("kind", json_u32(static_cast<std::uint32_t>(v.kind)));
    obj.add("hypervisor", json_str(v.hypervisor));
    obj.add("container", json_bool(v.container));
    return obj.build(pretty, indent);
}

[[nodiscard]] Virtualization virt_from_json(const detail::JsonVal& obj)
{
    Virtualization v;
    v.kind = static_cast<VirtualizationKind>(req_u64(obj, "kind"));
    v.hypervisor = req_str(obj, "hypervisor");
    v.container = req_bool(obj, "container");
    return v;
}

[[nodiscard]] std::string platform_to_json(const Platform& p, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("host", host_to_json(p.host, pretty, indent + 1));
    obj.add("os", os_to_json(p.os, pretty, indent + 1));
    obj.add("kernel", kernel_to_json(p.kernel, pretty, indent + 1));
    obj.add("architecture", arch_to_json(p.architecture, pretty, indent + 1));
    if(p.firmware)
    {
        obj.add("firmware", firmware_to_json(*p.firmware, pretty, indent + 1));
    }
    if(p.virtualization)
    {
        obj.add("virtualization", virt_to_json(*p.virtualization, pretty, indent + 1));
    }
    return obj.build(pretty, indent);
}

[[nodiscard]] Platform platform_from_json(const detail::JsonVal& obj)
{
    Platform p;
    p.host = host_from_json(req_obj(obj, "host"));
    p.os = os_from_json(req_obj(obj, "os"));
    p.kernel = kernel_from_json(req_obj(obj, "kernel"));
    p.architecture = arch_from_json(req_obj(obj, "architecture"));
    if(const auto* v = obj.get("firmware"))
    {
        if(v->type == detail::JsonVal::Type::Obj)
        {
            p.firmware = firmware_from_json(*v);
        }
    }
    if(const auto* v = obj.get("virtualization"))
    {
        if(v->type == detail::JsonVal::Type::Obj)
        {
            p.virtualization = virt_from_json(*v);
        }
    }
    return p;
}

// ──────────────────────── Cpu 序列化 ───────────────────────────────────────

[[nodiscard]] std::string cpu_package_to_json(const CpuPackage& pkg, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("id", json_u32(pkg.id.value()));
    obj.add("vendor", json_str(pkg.vendor.value));
    obj.add("model_name", json_str(pkg.model_name.value));
    obj.add("physical_cores", json_u32(pkg.physical_cores));
    obj.add("logical_threads", json_u32(pkg.logical_threads));
    if(pkg.base_frequency)
    {
        obj.add("base_frequency", json_u64(pkg.base_frequency->value));
    }
    if(pkg.max_frequency)
    {
        obj.add("max_frequency", json_u64(pkg.max_frequency->value));
    }
    return obj.build(pretty, indent);
}

[[nodiscard]] CpuPackage cpu_package_from_json(const detail::JsonVal& obj)
{
    CpuPackage pkg;
    pkg.id = CpuPackageId(static_cast<std::uint32_t>(req_u64(obj, "id")));
    pkg.vendor.value = req_str(obj, "vendor");
    pkg.model_name.value = req_str(obj, "model_name");
    pkg.physical_cores = static_cast<std::uint32_t>(req_u64(obj, "physical_cores"));
    pkg.logical_threads = static_cast<std::uint32_t>(req_u64(obj, "logical_threads"));
    if(auto v = opt_u64(obj, "base_frequency"))
    {
        pkg.base_frequency = Frequency{*v};
    }
    if(auto v = opt_u64(obj, "max_frequency"))
    {
        pkg.max_frequency = Frequency{*v};
    }
    return pkg;
}

[[nodiscard]] std::string cpu_core_to_json(const CpuCore& c, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("id", json_u32(c.id.value()));
    obj.add("package_id", json_u32(c.package_id.value()));
    obj.add("logical_threads", json_u32(c.logical_threads));
    if(c.numa_node)
    {
        obj.add("numa_node", json_u32(c.numa_node->value()));
    }
    return obj.build(pretty, indent);
}

[[nodiscard]] CpuCore cpu_core_from_json(const detail::JsonVal& obj)
{
    CpuCore c;
    c.id = CpuCoreId(static_cast<std::uint32_t>(req_u64(obj, "id")));
    c.package_id = CpuPackageId(static_cast<std::uint32_t>(req_u64(obj, "package_id")));
    c.logical_threads = static_cast<std::uint32_t>(req_u64(obj, "logical_threads"));
    if(auto v = opt_u64(obj, "numa_node"))
    {
        c.numa_node = NumaNodeId(static_cast<std::uint32_t>(*v));
    }
    return c;
}

[[nodiscard]] std::string logical_cpu_to_json(const LogicalCpu& lc, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("id", json_u32(lc.id.value()));
    obj.add("core_id", json_u32(lc.core_id.value()));
    obj.add("package_id", json_u32(lc.package_id.value()));
    if(lc.numa_node)
    {
        obj.add("numa_node", json_u32(lc.numa_node->value()));
    }
    obj.add("visible_to_current_process", json_bool(lc.visible_to_current_process));
    return obj.build(pretty, indent);
}

[[nodiscard]] LogicalCpu logical_cpu_from_json(const detail::JsonVal& obj)
{
    LogicalCpu lc;
    lc.id = LogicalCpuId(static_cast<std::uint32_t>(req_u64(obj, "id")));
    lc.core_id = CpuCoreId(static_cast<std::uint32_t>(req_u64(obj, "core_id")));
    lc.package_id = CpuPackageId(static_cast<std::uint32_t>(req_u64(obj, "package_id")));
    if(auto v = opt_u64(obj, "numa_node"))
    {
        lc.numa_node = NumaNodeId(static_cast<std::uint32_t>(*v));
    }
    lc.visible_to_current_process = req_bool(obj, "visible_to_current_process");
    return lc;
}

[[nodiscard]] std::string numa_node_to_json(const NumaNode& n, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("id", json_u32(n.id.value()));
    detail::JsonArr arr;
    for(const auto& cpu_id : n.cpus)
    {
        arr.add(json_u32(cpu_id.value()));
    }
    obj.add("cpus", arr.build(pretty, indent + 1));
    return obj.build(pretty, indent);
}

[[nodiscard]] NumaNode numa_node_from_json(const detail::JsonVal& obj)
{
    NumaNode n;
    n.id = NumaNodeId(static_cast<std::uint32_t>(req_u64(obj, "id")));
    const auto& cpus_arr = req_arr(obj, "cpus");
    for(const auto& elem : cpus_arr.arr_val)
    {
        auto v = elem.as_u64();
        if(!v)
        {
            throw SysalError(ErrorKind::DeserializationError, "NUMA node cpus must be integers");
        }
        n.cpus.push_back(LogicalCpuId(static_cast<std::uint32_t>(*v)));
    }
    return n;
}

[[nodiscard]] std::string cpu_to_json(const Cpu& c, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("arch", json_u32(static_cast<std::uint32_t>(c.arch)));

    detail::JsonArr pkg_arr;
    for(const auto& pkg : c.packages)
    {
        pkg_arr.add(cpu_package_to_json(pkg, pretty, indent + 2));
    }
    obj.add("packages", pkg_arr.build(pretty, indent + 1));

    detail::JsonArr core_arr;
    for(const auto& core : c.cores)
    {
        core_arr.add(cpu_core_to_json(core, pretty, indent + 2));
    }
    obj.add("cores", core_arr.build(pretty, indent + 1));

    detail::JsonArr lc_arr;
    for(const auto& lc : c.logical_cpus)
    {
        lc_arr.add(logical_cpu_to_json(lc, pretty, indent + 2));
    }
    obj.add("logical_cpus", lc_arr.build(pretty, indent + 1));

    detail::JsonArr numa_arr;
    for(const auto& n : c.numa_nodes)
    {
        numa_arr.add(numa_node_to_json(n, pretty, indent + 2));
    }
    obj.add("numa_nodes", numa_arr.build(pretty, indent + 1));

    detail::JsonArr isa_arr;
    for(const auto& ext : c.isa_extensions)
    {
        isa_arr.add(json_u32(static_cast<std::uint32_t>(ext)));
    }
    obj.add("isa_extensions", isa_arr.build(pretty, indent + 1));

    return obj.build(pretty, indent);
}

[[nodiscard]] Cpu cpu_from_json(const detail::JsonVal& obj)
{
    Cpu c;
    c.arch = static_cast<Arch>(req_u64(obj, "arch"));

    const auto& pkg_arr = req_arr(obj, "packages");
    for(const auto& elem : pkg_arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError, "CPU package must be an object");
        }
        c.packages.push_back(cpu_package_from_json(elem));
    }

    const auto& core_arr = req_arr(obj, "cores");
    for(const auto& elem : core_arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError, "CPU core must be an object");
        }
        c.cores.push_back(cpu_core_from_json(elem));
    }

    const auto& lc_arr = req_arr(obj, "logical_cpus");
    for(const auto& elem : lc_arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError, "logical CPU must be an object");
        }
        c.logical_cpus.push_back(logical_cpu_from_json(elem));
    }

    const auto& numa_arr = req_arr(obj, "numa_nodes");
    for(const auto& elem : numa_arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError, "NUMA node must be an object");
        }
        c.numa_nodes.push_back(numa_node_from_json(elem));
    }

    const auto& isa_arr = req_arr(obj, "isa_extensions");
    for(const auto& elem : isa_arr.arr_val)
    {
        auto v = elem.as_u64();
        if(!v)
        {
            throw SysalError(ErrorKind::DeserializationError, "ISA extension must be an integer");
        }
        c.isa_extensions.push_back(static_cast<IsaExtension>(*v));
    }

    return c;
}

// ──────────────────────── Memory 序列化 ────────────────────────────────────

[[nodiscard]] std::string numa_memory_to_json(const NumaMemory& nm, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("node", json_u32(nm.node.value()));
    obj.add("total", json_u64(nm.total.value));
    if(nm.available)
    {
        obj.add("available", json_u64(nm.available->value));
    }
    return obj.build(pretty, indent);
}

[[nodiscard]] NumaMemory numa_memory_from_json(const detail::JsonVal& obj)
{
    NumaMemory nm;
    nm.node = NumaNodeId(static_cast<std::uint32_t>(req_u64(obj, "node")));
    nm.total = MemorySize{req_u64(obj, "total")};
    if(auto v = opt_u64(obj, "available"))
    {
        nm.available = MemorySize{*v};
    }
    return nm;
}

[[nodiscard]] std::string memory_to_json(const Memory& m, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("total_memory", json_u64(m.total_memory.value));
    if(m.available_memory)
    {
        obj.add("available_memory", json_u64(m.available_memory->value));
    }
    detail::JsonArr arr;
    for(const auto& nm : m.numa_memory)
    {
        arr.add(numa_memory_to_json(nm, pretty, indent + 2));
    }
    obj.add("numa_memory", arr.build(pretty, indent + 1));
    return obj.build(pretty, indent);
}

[[nodiscard]] Memory memory_from_json(const detail::JsonVal& obj)
{
    Memory m;
    m.total_memory = MemorySize{req_u64(obj, "total_memory")};
    if(auto v = opt_u64(obj, "available_memory"))
    {
        m.available_memory = MemorySize{*v};
    }
    const auto* numa_val = obj.get("numa_memory");
    if(numa_val && numa_val->type == detail::JsonVal::Type::Arr)
    {
        for(const auto& elem : numa_val->arr_val)
        {
            if(elem.type != detail::JsonVal::Type::Obj)
            {
                throw SysalError(ErrorKind::DeserializationError,
                                 "NUMA memory entry must be an object");
            }
            m.numa_memory.push_back(numa_memory_from_json(elem));
        }
    }
    return m;
}

// ──────────────────────── Accelerator 序列化 ───────────────────────────────

[[nodiscard]] std::string accel_device_to_json(const AcceleratorDevice& d, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("id", json_u32(d.id.value()));
    obj.add("kind", json_u32(static_cast<std::uint32_t>(d.kind)));
    obj.add("vendor", json_str(d.vendor.value));
    obj.add("name", json_str(d.name.value));
    if(d.pci_address)
    {
        obj.add("pci_address", pci_address_to_json(*d.pci_address, pretty, indent + 1));
    }
    if(d.nearest_numa_node)
    {
        obj.add("nearest_numa_node", json_u32(d.nearest_numa_node->value()));
    }
    if(d.memory_size)
    {
        obj.add("memory_size", json_u64(d.memory_size->value));
    }
    if(d.driver)
    {
        obj.add("driver", json_u32(d.driver->value()));
    }
    obj.add("visible_to_current_process", json_bool(d.visible_to_current_process));
    return obj.build(pretty, indent);
}

[[nodiscard]] AcceleratorDevice accel_device_from_json(const detail::JsonVal& obj)
{
    AcceleratorDevice d;
    d.id = AcceleratorId(static_cast<std::uint32_t>(req_u64(obj, "id")));
    d.kind = static_cast<AcceleratorKind>(req_u64(obj, "kind"));
    d.vendor.value = req_str(obj, "vendor");
    d.name.value = req_str(obj, "name");
    if(const auto* v = obj.get("pci_address"))
    {
        if(v->type == detail::JsonVal::Type::Obj)
        {
            d.pci_address = pci_address_from_json(*v);
        }
    }
    if(auto v = opt_u64(obj, "nearest_numa_node"))
    {
        d.nearest_numa_node = NumaNodeId(static_cast<std::uint32_t>(*v));
    }
    if(auto v = opt_u64(obj, "memory_size"))
    {
        d.memory_size = MemorySize{*v};
    }
    if(auto v = opt_u64(obj, "driver"))
    {
        d.driver = DriverId(static_cast<std::uint32_t>(*v));
    }
    d.visible_to_current_process = req_bool(obj, "visible_to_current_process");
    return d;
}

[[nodiscard]] std::string accelerators_to_json(const Accelerators& a, bool pretty, int indent)
{
    detail::JsonArr arr;
    for(const auto& dev : a.devices)
    {
        arr.add(accel_device_to_json(dev, pretty, indent + 2));
    }
    detail::JsonObj obj;
    obj.add("devices", arr.build(pretty, indent + 1));
    return obj.build(pretty, indent);
}

[[nodiscard]] Accelerators accelerators_from_json(const detail::JsonVal& obj)
{
    Accelerators a;
    const auto& dev_arr = req_arr(obj, "devices");
    for(const auto& elem : dev_arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError,
                             "accelerator device must be an object");
        }
        a.devices.push_back(accel_device_from_json(elem));
    }
    return a;
}

// ──────────────────────── Network 序列化 ───────────────────────────────────

[[nodiscard]] std::string net_iface_to_json(const NetworkInterface& ni, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("name", json_str(ni.name.value));
    obj.add("mac", json_str(ni.mac.value));
    obj.add("state", json_u32(static_cast<std::uint32_t>(ni.state)));
    if(ni.speed)
    {
        obj.add("speed", json_u64(ni.speed->value));
    }
    detail::JsonArr addr_arr;
    for(const auto& addr : ni.addresses)
    {
        addr_arr.add(json_str(addr.value));
    }
    obj.add("addresses", addr_arr.build(pretty, indent + 1));
    if(ni.pci_address)
    {
        obj.add("pci_address", pci_address_to_json(*ni.pci_address, pretty, indent + 1));
    }
    obj.add("visible_to_current_process", json_bool(ni.visible_to_current_process));
    return obj.build(pretty, indent);
}

[[nodiscard]] NetworkInterface net_iface_from_json(const detail::JsonVal& obj)
{
    NetworkInterface ni;
    ni.name.value = req_str(obj, "name");
    ni.mac.value = req_str(obj, "mac");
    ni.state = static_cast<InterfaceState>(req_u64(obj, "state"));
    if(auto v = opt_u64(obj, "speed"))
    {
        ni.speed = Bandwidth{*v};
    }
    const auto* addr_val = obj.get("addresses");
    if(addr_val && addr_val->type == detail::JsonVal::Type::Arr)
    {
        for(const auto& elem : addr_val->arr_val)
        {
            if(elem.type == detail::JsonVal::Type::Str)
            {
                IpAddress ip;
                ip.value = elem.str_val;
                ni.addresses.push_back(std::move(ip));
            }
        }
    }
    if(const auto* v = obj.get("pci_address"))
    {
        if(v->type == detail::JsonVal::Type::Obj)
        {
            ni.pci_address = pci_address_from_json(*v);
        }
    }
    ni.visible_to_current_process = req_bool(obj, "visible_to_current_process");
    return ni;
}

[[nodiscard]] std::string network_to_json(const Network& n, bool pretty, int indent)
{
    detail::JsonArr arr;
    for(const auto& iface : n.interfaces)
    {
        arr.add(net_iface_to_json(iface, pretty, indent + 2));
    }
    detail::JsonObj obj;
    obj.add("interfaces", arr.build(pretty, indent + 1));
    return obj.build(pretty, indent);
}

[[nodiscard]] Network network_from_json(const detail::JsonVal& obj)
{
    Network n;
    const auto& if_arr = req_arr(obj, "interfaces");
    for(const auto& elem : if_arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError,
                             "network interface must be an object");
        }
        n.interfaces.push_back(net_iface_from_json(elem));
    }
    return n;
}

// ──────────────────────── Storage 序列化 ───────────────────────────────────

[[nodiscard]] std::string storage_dev_to_json(const StorageDevice& sd, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("id", json_u32(sd.id.value()));
    obj.add("name", json_str(sd.name.value));
    if(sd.capacity)
    {
        obj.add("capacity", json_u64(sd.capacity->value));
    }
    if(sd.pci_address)
    {
        obj.add("pci_address", pci_address_to_json(*sd.pci_address, pretty, indent + 1));
    }
    obj.add("kind", json_u32(static_cast<std::uint32_t>(sd.kind)));
    return obj.build(pretty, indent);
}

[[nodiscard]] StorageDevice storage_dev_from_json(const detail::JsonVal& obj)
{
    StorageDevice sd;
    sd.id = StorageId(static_cast<std::uint32_t>(req_u64(obj, "id")));
    sd.name.value = req_str(obj, "name");
    if(auto v = opt_u64(obj, "capacity"))
    {
        sd.capacity = MemorySize{*v};
    }
    if(const auto* v = obj.get("pci_address"))
    {
        if(v->type == detail::JsonVal::Type::Obj)
        {
            sd.pci_address = pci_address_from_json(*v);
        }
    }
    sd.kind = static_cast<StorageKind>(req_u64(obj, "kind"));
    return sd;
}

[[nodiscard]] std::string storage_to_json(const Storage& s, bool pretty, int indent)
{
    detail::JsonArr arr;
    for(const auto& dev : s.devices)
    {
        arr.add(storage_dev_to_json(dev, pretty, indent + 2));
    }
    detail::JsonObj obj;
    obj.add("devices", arr.build(pretty, indent + 1));
    return obj.build(pretty, indent);
}

[[nodiscard]] Storage storage_from_json(const detail::JsonVal& obj)
{
    Storage s;
    const auto& dev_arr = req_arr(obj, "devices");
    for(const auto& elem : dev_arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError, "storage device must be an object");
        }
        s.devices.push_back(storage_dev_from_json(elem));
    }
    return s;
}

// ──────────────────────── Pci 序列化 ───────────────────────────────────────

[[nodiscard]] std::string pci_device_to_json(const PciDevice& pd, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("address", pci_address_to_json(pd.address, pretty, indent + 1));
    obj.add("vendor", json_str(pd.vendor.value));
    obj.add("device_name", json_str(pd.device_name.value));
    obj.add("device_class", json_str(pd.device_class.value));
    if(pd.numa_node)
    {
        obj.add("numa_node", json_u32(pd.numa_node->value()));
    }
    return obj.build(pretty, indent);
}

[[nodiscard]] PciDevice pci_device_from_json(const detail::JsonVal& obj)
{
    PciDevice pd;
    pd.address = pci_address_from_json(req_obj(obj, "address"));
    pd.vendor.value = req_str(obj, "vendor");
    pd.device_name.value = req_str(obj, "device_name");
    pd.device_class.value = req_str(obj, "device_class");
    if(auto v = opt_u64(obj, "numa_node"))
    {
        pd.numa_node = NumaNodeId(static_cast<std::uint32_t>(*v));
    }
    return pd;
}

[[nodiscard]] std::string pci_to_json(const Pci& p, bool pretty, int indent)
{
    detail::JsonArr arr;
    for(const auto& dev : p.devices)
    {
        arr.add(pci_device_to_json(dev, pretty, indent + 2));
    }
    detail::JsonObj obj;
    obj.add("devices", arr.build(pretty, indent + 1));
    return obj.build(pretty, indent);
}

[[nodiscard]] Pci pci_from_json(const detail::JsonVal& obj)
{
    Pci p;
    const auto& dev_arr = req_arr(obj, "devices");
    for(const auto& elem : dev_arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError, "PCI device must be an object");
        }
        p.devices.push_back(pci_device_from_json(elem));
    }
    return p;
}

// ──────────────────────── Software 序列化 ──────────────────────────────────

[[nodiscard]] std::string driver_to_json(const Driver& d, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("id", json_u32(d.id.value()));
    obj.add("name", json_str(d.name));
    obj.add("version", json_str(d.version));
    obj.add("loaded", json_bool(d.loaded));
    obj.add("path", json_str(d.path));
    return obj.build(pretty, indent);
}

[[nodiscard]] Driver driver_from_json(const detail::JsonVal& obj)
{
    Driver d;
    d.id = DriverId(static_cast<std::uint32_t>(req_u64(obj, "id")));
    d.name = req_str(obj, "name");
    d.version = req_str(obj, "version");
    d.loaded = req_bool(obj, "loaded");
    d.path = req_str(obj, "path");
    return d;
}

[[nodiscard]] std::string runtime_to_json(const Runtime& r, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("name", json_str(r.name));
    obj.add("version", json_str(r.version));
    obj.add("path", json_str(r.path));
    obj.add("env_var", json_str(r.env_var));
    return obj.build(pretty, indent);
}

[[nodiscard]] Runtime runtime_from_json(const detail::JsonVal& obj)
{
    Runtime r;
    r.name = req_str(obj, "name");
    r.version = req_str(obj, "version");
    r.path = req_str(obj, "path");
    r.env_var = req_str(obj, "env_var");
    return r;
}

[[nodiscard]] std::string compiler_to_json(const Compiler& c, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("name", json_str(c.name));
    obj.add("version", json_str(c.version));
    obj.add("path", json_str(c.path));
    obj.add("target", json_str(c.target));
    return obj.build(pretty, indent);
}

[[nodiscard]] Compiler compiler_from_json(const detail::JsonVal& obj)
{
    Compiler c;
    c.name = req_str(obj, "name");
    c.version = req_str(obj, "version");
    c.path = req_str(obj, "path");
    c.target = req_str(obj, "target");
    return c;
}

[[nodiscard]] std::string library_to_json(const Library& l, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("name", json_str(l.name));
    obj.add("version", json_str(l.version));
    obj.add("path", json_str(l.path));
    obj.add("kind", json_str(l.kind));
    return obj.build(pretty, indent);
}

[[nodiscard]] Library library_from_json(const detail::JsonVal& obj)
{
    Library l;
    l.name = req_str(obj, "name");
    l.version = req_str(obj, "version");
    l.path = req_str(obj, "path");
    l.kind = req_str(obj, "kind");
    return l;
}

[[nodiscard]] std::string cuda_to_json(const Cuda& c, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("version", json_str(c.version));
    obj.add("driver_version", json_str(c.driver_version));
    obj.add("nvcc_path", json_str(c.nvcc_path));
    obj.add("home", json_str(c.home));
    return obj.build(pretty, indent);
}

[[nodiscard]] Cuda cuda_from_json(const detail::JsonVal& obj)
{
    Cuda c;
    c.version = req_str(obj, "version");
    c.driver_version = req_str(obj, "driver_version");
    c.nvcc_path = req_str(obj, "nvcc_path");
    c.home = req_str(obj, "home");
    return c;
}

[[nodiscard]] std::string rocm_to_json(const Rocm& r, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("version", json_str(r.version));
    obj.add("hip_path", json_str(r.hip_path));
    obj.add("rocm_path", json_str(r.rocm_path));
    return obj.build(pretty, indent);
}

[[nodiscard]] Rocm rocm_from_json(const detail::JsonVal& obj)
{
    Rocm r;
    r.version = req_str(obj, "version");
    r.hip_path = req_str(obj, "hip_path");
    r.rocm_path = req_str(obj, "rocm_path");
    return r;
}

[[nodiscard]] std::string level_zero_to_json(const LevelZero& lz, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("version", json_str(lz.version));
    obj.add("loader_path", json_str(lz.loader_path));
    return obj.build(pretty, indent);
}

[[nodiscard]] LevelZero level_zero_from_json(const detail::JsonVal& obj)
{
    LevelZero lz;
    lz.version = req_str(obj, "version");
    lz.loader_path = req_str(obj, "loader_path");
    return lz;
}

[[nodiscard]] std::string mpi_to_json(const Mpi& m, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("implementation", json_str(m.implementation));
    obj.add("version", json_str(m.version));
    obj.add("path", json_str(m.path));
    return obj.build(pretty, indent);
}

[[nodiscard]] Mpi mpi_from_json(const detail::JsonVal& obj)
{
    Mpi m;
    m.implementation = req_str(obj, "implementation");
    m.version = req_str(obj, "version");
    m.path = req_str(obj, "path");
    return m;
}

[[nodiscard]] std::string rdma_to_json(const RdmaStack& r, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("rdma_core_version", json_str(r.rdma_core_version));
    obj.add("ibverbs_path", json_str(r.ibverbs_path));
    obj.add("ucx_version", json_str(r.ucx_version));
    return obj.build(pretty, indent);
}

[[nodiscard]] RdmaStack rdma_from_json(const detail::JsonVal& obj)
{
    RdmaStack r;
    r.rdma_core_version = req_str(obj, "rdma_core_version");
    r.ibverbs_path = req_str(obj, "ibverbs_path");
    r.ucx_version = req_str(obj, "ucx_version");
    return r;
}

[[nodiscard]] std::string software_to_json(const SoftwareStack& sw, bool pretty, int indent)
{
    detail::JsonObj obj;

    detail::JsonArr drv_arr;
    for(const auto& d : sw.drivers)
    {
        drv_arr.add(driver_to_json(d, pretty, indent + 2));
    }
    obj.add("drivers", drv_arr.build(pretty, indent + 1));

    detail::JsonArr rt_arr;
    for(const auto& r : sw.runtimes)
    {
        rt_arr.add(runtime_to_json(r, pretty, indent + 2));
    }
    obj.add("runtimes", rt_arr.build(pretty, indent + 1));

    detail::JsonArr cc_arr;
    for(const auto& c : sw.compilers)
    {
        cc_arr.add(compiler_to_json(c, pretty, indent + 2));
    }
    obj.add("compilers", cc_arr.build(pretty, indent + 1));

    detail::JsonArr lib_arr;
    for(const auto& l : sw.libraries)
    {
        lib_arr.add(library_to_json(l, pretty, indent + 2));
    }
    obj.add("libraries", lib_arr.build(pretty, indent + 1));

    if(sw.cuda)
    {
        obj.add("cuda", cuda_to_json(*sw.cuda, pretty, indent + 1));
    }
    if(sw.rocm)
    {
        obj.add("rocm", rocm_to_json(*sw.rocm, pretty, indent + 1));
    }
    if(sw.level_zero)
    {
        obj.add("level_zero", level_zero_to_json(*sw.level_zero, pretty, indent + 1));
    }
    if(sw.mpi)
    {
        obj.add("mpi", mpi_to_json(*sw.mpi, pretty, indent + 1));
    }
    if(sw.rdma)
    {
        obj.add("rdma", rdma_to_json(*sw.rdma, pretty, indent + 1));
    }

    return obj.build(pretty, indent);
}

[[nodiscard]] SoftwareStack software_from_json(const detail::JsonVal& obj)
{
    SoftwareStack sw;

    const auto& drv_arr = req_arr(obj, "drivers");
    for(const auto& elem : drv_arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError, "driver must be an object");
        }
        sw.drivers.push_back(driver_from_json(elem));
    }

    const auto& rt_arr = req_arr(obj, "runtimes");
    for(const auto& elem : rt_arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError, "runtime must be an object");
        }
        sw.runtimes.push_back(runtime_from_json(elem));
    }

    const auto& cc_arr = req_arr(obj, "compilers");
    for(const auto& elem : cc_arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError, "compiler must be an object");
        }
        sw.compilers.push_back(compiler_from_json(elem));
    }

    const auto& lib_arr = req_arr(obj, "libraries");
    for(const auto& elem : lib_arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError, "library must be an object");
        }
        sw.libraries.push_back(library_from_json(elem));
    }

    if(const auto* v = obj.get("cuda"))
    {
        if(v->type == detail::JsonVal::Type::Obj)
        {
            sw.cuda = cuda_from_json(*v);
        }
    }
    if(const auto* v = obj.get("rocm"))
    {
        if(v->type == detail::JsonVal::Type::Obj)
        {
            sw.rocm = rocm_from_json(*v);
        }
    }
    if(const auto* v = obj.get("level_zero"))
    {
        if(v->type == detail::JsonVal::Type::Obj)
        {
            sw.level_zero = level_zero_from_json(*v);
        }
    }
    if(const auto* v = obj.get("mpi"))
    {
        if(v->type == detail::JsonVal::Type::Obj)
        {
            sw.mpi = mpi_from_json(*v);
        }
    }
    if(const auto* v = obj.get("rdma"))
    {
        if(v->type == detail::JsonVal::Type::Obj)
        {
            sw.rdma = rdma_from_json(*v);
        }
    }

    return sw;
}

// ──────────────────────── Execution 序列化 ─────────────────────────────────

[[nodiscard]] std::string process_to_json(const Process& p, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("pid", json_i32(p.pid));
    obj.add("ppid", json_i32(p.ppid));
    obj.add("uid", json_u32(p.uid));
    obj.add("gid", json_u32(p.gid));
    obj.add("comm", json_str(p.comm));
    obj.add("exe", json_str(p.exe));
    obj.add("cwd", json_str(p.cwd));
    return obj.build(pretty, indent);
}

[[nodiscard]] Process process_from_json(const detail::JsonVal& obj)
{
    Process p;
    p.pid = static_cast<std::int32_t>(req_i64(obj, "pid"));
    p.ppid = static_cast<std::int32_t>(req_i64(obj, "ppid"));
    p.uid = static_cast<std::uint32_t>(req_u64(obj, "uid"));
    p.gid = static_cast<std::uint32_t>(req_u64(obj, "gid"));
    p.comm = req_str(obj, "comm");
    p.exe = req_str(obj, "exe");
    p.cwd = req_str(obj, "cwd");
    return p;
}

[[nodiscard]] std::string environment_to_json(const Environment& e, bool pretty, int indent)
{
    detail::JsonArr arr;
    for(const auto& [k, v] : e.entries)
    {
        detail::JsonObj pair_obj;
        pair_obj.add("key", json_str(k));
        pair_obj.add("value", json_str(v));
        arr.add(pair_obj.build(pretty, indent + 2));
    }
    detail::JsonObj obj;
    obj.add("entries", arr.build(pretty, indent + 1));
    return obj.build(pretty, indent);
}

[[nodiscard]] Environment environment_from_json(const detail::JsonVal& obj)
{
    Environment e;
    const auto& arr = req_arr(obj, "entries");
    for(const auto& elem : arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError,
                             "environment entry must be an object");
        }
        auto key = req_str(elem, "key");
        auto val = req_str(elem, "value");
        e.entries.emplace_back(std::move(key), std::move(val));
    }
    return e;
}

[[nodiscard]] std::string cgroup_to_json(const Cgroup& c, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("version", json_u32(static_cast<std::uint32_t>(c.version)));
    obj.add("path", json_str(c.path));
    detail::JsonArr arr;
    for(const auto& ctrl : c.controllers)
    {
        arr.add(json_str(ctrl));
    }
    obj.add("controllers", arr.build(pretty, indent + 1));
    return obj.build(pretty, indent);
}

[[nodiscard]] Cgroup cgroup_from_json(const detail::JsonVal& obj)
{
    Cgroup c;
    c.version = static_cast<CgroupVersion>(req_u64(obj, "version"));
    c.path = req_str(obj, "path");
    const auto* ctrl_val = obj.get("controllers");
    if(ctrl_val && ctrl_val->type == detail::JsonVal::Type::Arr)
    {
        c.controllers = str_array_from_json(*ctrl_val);
    }
    return c;
}

[[nodiscard]] std::string cpuset_to_json(const Cpuset& cs, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("cpus", json_str(cs.cpus));
    obj.add("mems", json_str(cs.mems));
    obj.add("cpus_effective", json_str(cs.cpus_effective));
    obj.add("mems_effective", json_str(cs.mems_effective));
    return obj.build(pretty, indent);
}

[[nodiscard]] Cpuset cpuset_from_json(const detail::JsonVal& obj)
{
    Cpuset cs;
    cs.cpus = req_str(obj, "cpus");
    cs.mems = req_str(obj, "mems");
    cs.cpus_effective = req_str(obj, "cpus_effective");
    cs.mems_effective = req_str(obj, "mems_effective");
    return cs;
}

[[nodiscard]] std::string permission_to_json(const Permission& p, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("euid", json_u32(p.euid));
    obj.add("egid", json_u32(p.egid));
    detail::JsonArr arr;
    for(const auto& cap : p.capabilities)
    {
        arr.add(json_str(cap));
    }
    obj.add("capabilities", arr.build(pretty, indent + 1));
    obj.add("is_root", json_bool(p.is_root));
    return obj.build(pretty, indent);
}

[[nodiscard]] Permission permission_from_json(const detail::JsonVal& obj)
{
    Permission p;
    p.euid = static_cast<std::uint32_t>(req_u64(obj, "euid"));
    p.egid = static_cast<std::uint32_t>(req_u64(obj, "egid"));
    const auto* cap_val = obj.get("capabilities");
    if(cap_val && cap_val->type == detail::JsonVal::Type::Arr)
    {
        p.capabilities = str_array_from_json(*cap_val);
    }
    p.is_root = req_bool(obj, "is_root");
    return p;
}

[[nodiscard]] std::string container_to_json(const Container& c, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("kind", json_u32(static_cast<std::uint32_t>(c.kind)));
    obj.add("id", json_str(c.id));
    obj.add("runtime", json_str(c.runtime));
    return obj.build(pretty, indent);
}

[[nodiscard]] Container container_from_json(const detail::JsonVal& obj)
{
    Container c;
    c.kind = static_cast<ContainerKind>(req_u64(obj, "kind"));
    c.id = req_str(obj, "id");
    c.runtime = req_str(obj, "runtime");
    return c;
}

[[nodiscard]] std::string execution_to_json(const ExecutionContext& e, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("process", process_to_json(e.process, pretty, indent + 1));
    obj.add("environment", environment_to_json(e.environment, pretty, indent + 1));
    obj.add("cgroup", cgroup_to_json(e.cgroup, pretty, indent + 1));
    obj.add("cpuset", cpuset_to_json(e.cpuset, pretty, indent + 1));
    obj.add("permission", permission_to_json(e.permission, pretty, indent + 1));
    if(e.container)
    {
        obj.add("container", container_to_json(*e.container, pretty, indent + 1));
    }

    detail::JsonArr vcpu_arr;
    for(const auto& id : e.visible_logical_cpu_ids)
    {
        vcpu_arr.add(json_u32(id.value()));
    }
    obj.add("visible_logical_cpu_ids", vcpu_arr.build(pretty, indent + 1));

    detail::JsonArr vacc_arr;
    for(const auto& id : e.visible_accelerator_ids)
    {
        vacc_arr.add(json_u32(id.value()));
    }
    obj.add("visible_accelerator_ids", vacc_arr.build(pretty, indent + 1));

    detail::JsonArr vnet_arr;
    for(const auto& name : e.visible_network_interface_names)
    {
        vnet_arr.add(json_str(name.value));
    }
    obj.add("visible_network_interface_names", vnet_arr.build(pretty, indent + 1));

    return obj.build(pretty, indent);
}

[[nodiscard]] ExecutionContext execution_from_json(const detail::JsonVal& obj)
{
    ExecutionContext e;
    e.process = process_from_json(req_obj(obj, "process"));
    e.environment = environment_from_json(req_obj(obj, "environment"));
    e.cgroup = cgroup_from_json(req_obj(obj, "cgroup"));
    e.cpuset = cpuset_from_json(req_obj(obj, "cpuset"));
    e.permission = permission_from_json(req_obj(obj, "permission"));

    if(const auto* v = obj.get("container"))
    {
        if(v->type == detail::JsonVal::Type::Obj)
        {
            e.container = container_from_json(*v);
        }
    }

    const auto& vcpu_arr = req_arr(obj, "visible_logical_cpu_ids");
    for(const auto& elem : vcpu_arr.arr_val)
    {
        auto v = elem.as_u64();
        if(!v)
        {
            throw SysalError(ErrorKind::DeserializationError,
                             "visible_logical_cpu_ids must be integers");
        }
        e.visible_logical_cpu_ids.push_back(LogicalCpuId(static_cast<std::uint32_t>(*v)));
    }

    const auto& vacc_arr = req_arr(obj, "visible_accelerator_ids");
    for(const auto& elem : vacc_arr.arr_val)
    {
        auto v = elem.as_u64();
        if(!v)
        {
            throw SysalError(ErrorKind::DeserializationError,
                             "visible_accelerator_ids must be integers");
        }
        e.visible_accelerator_ids.push_back(AcceleratorId(static_cast<std::uint32_t>(*v)));
    }

    const auto& vnet_arr = req_arr(obj, "visible_network_interface_names");
    for(const auto& elem : vnet_arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Str)
        {
            throw SysalError(ErrorKind::DeserializationError,
                             "visible_network_interface_names must be strings");
        }
        InterfaceName in;
        in.value = elem.str_val;
        e.visible_network_interface_names.push_back(std::move(in));
    }

    return e;
}

// ──────────────────────── SystemInfo 序列化 ────────────────────────────────

[[nodiscard]] std::string system_info_to_json(const SystemInfo& info, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("platform", platform_to_json(info.platform, pretty, indent + 1));
    obj.add("cpu", cpu_to_json(info.cpu, pretty, indent + 1));
    obj.add("memory", memory_to_json(info.memory, pretty, indent + 1));
    obj.add("accelerators", accelerators_to_json(info.accelerators, pretty, indent + 1));
    obj.add("network", network_to_json(info.network, pretty, indent + 1));
    obj.add("storage", storage_to_json(info.storage, pretty, indent + 1));
    obj.add("pci", pci_to_json(info.pci, pretty, indent + 1));
    obj.add("software", software_to_json(info.software, pretty, indent + 1));
    obj.add("execution", execution_to_json(info.execution, pretty, indent + 1));
    return obj.build(pretty, indent);
}

[[nodiscard]] SystemInfo system_info_from_json(const detail::JsonVal& obj)
{
    SystemInfo info;
    info.platform = platform_from_json(req_obj(obj, "platform"));
    info.cpu = cpu_from_json(req_obj(obj, "cpu"));
    info.memory = memory_from_json(req_obj(obj, "memory"));
    info.accelerators = accelerators_from_json(req_obj(obj, "accelerators"));
    info.network = network_from_json(req_obj(obj, "network"));
    info.storage = storage_from_json(req_obj(obj, "storage"));
    info.pci = pci_from_json(req_obj(obj, "pci"));
    info.software = software_from_json(req_obj(obj, "software"));
    info.execution = execution_from_json(req_obj(obj, "execution"));
    return info;
}

// ──────────────────────── SnapshotMeta 序列化 ──────────────────────────────

[[nodiscard]] std::string meta_to_json(const SnapshotMeta& m, bool pretty, int indent)
{
    detail::JsonObj obj;
    obj.add("collect_time", detail::time_point_to_ms(m.collect_time));
    obj.add("sysal_version", json_str(m.sysal_version));
    obj.add("collect_duration", json_double(m.collect_duration.count()));
    obj.add("requested_flags", json_u32(static_cast<std::uint32_t>(m.requested_flags)));

    detail::JsonArr succ_arr;
    for(const auto& s : m.succeeded_collectors)
    {
        succ_arr.add(json_str(s));
    }
    obj.add("succeeded_collectors", succ_arr.build(pretty, indent + 1));

    detail::JsonArr fail_arr;
    for(const auto& f : m.failed_collectors)
    {
        fail_arr.add(json_str(f));
    }
    obj.add("failed_collectors", fail_arr.build(pretty, indent + 1));

    return obj.build(pretty, indent);
}

[[nodiscard]] SnapshotMeta meta_from_json(const detail::JsonVal& obj)
{
    SnapshotMeta m;
    auto time_ms = req_i64(obj, "collect_time");
    m.collect_time = detail::ms_to_time_point(time_ms);
    m.sysal_version = req_str(obj, "sysal_version");
    m.collect_duration = std::chrono::duration<double>(req_double(obj, "collect_duration"));
    m.requested_flags = static_cast<Collect>(req_u64(obj, "requested_flags"));

    const auto& succ_arr = req_arr(obj, "succeeded_collectors");
    m.succeeded_collectors = str_array_from_json(succ_arr);

    const auto& fail_arr = req_arr(obj, "failed_collectors");
    m.failed_collectors = str_array_from_json(fail_arr);

    return m;
}

// ──────────────────────── RawStore (System 级) 序列化 ──────────────────────

[[nodiscard]] std::string raw_store_to_system_json(const RawStore& store, bool pretty, int indent)
{
    detail::JsonArr arr;
    for(const auto& rec : store.records)
    {
        detail::JsonObj obj;
        obj.add("source", std::to_string(static_cast<std::uint64_t>(rec.source)));
        obj.add("path_or_command", detail::escape_string(rec.path_or_command));
        obj.add("payload", detail::escape_string(rec.payload));
        obj.add("status", std::to_string(static_cast<std::uint64_t>(rec.status)));
        obj.add("collected_at", detail::time_point_to_ms(rec.collected_at));
        arr.add(obj.build(pretty, indent + 2));
    }
    detail::JsonObj root;
    root.add("records", arr.build(pretty, indent + 1));
    return root.build(pretty, indent);
}

[[nodiscard]] RawRecord sys_raw_record_from_json(const detail::JsonVal& obj)
{
    RawRecord rec;
    rec.source = static_cast<RawSource>(req_u64(obj, "source"));
    rec.path_or_command = req_str(obj, "path_or_command");
    rec.payload = req_str(obj, "payload");
    rec.status = static_cast<CollectStatus>(req_u64(obj, "status"));
    rec.collected_at = detail::ms_to_time_point(req_i64(obj, "collected_at"));
    return rec;
}

[[nodiscard]] RawStore sys_raw_store_from_json(const detail::JsonVal& root)
{
    const auto& records_arr = req_arr(root, "records");
    RawStore store;
    for(const auto& elem : records_arr.arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError,
                             "each element in 'records' must be a JSON object");
        }
        store.records.push_back(sys_raw_record_from_json(elem));
    }
    return store;
}

} // namespace

// ══════════════════════════════════════════════════════════════════════════
// 公共接口
// ══════════════════════════════════════════════════════════════════════════

std::string to_json(const System& sys, const SerializationOptions& opts)
{
    const bool pretty = opts.pretty_print;

    detail::JsonObj root;
    root.add("info", system_info_to_json(sys.info, pretty, 1));

    if(opts.include_meta)
    {
        root.add("meta", meta_to_json(sys.meta, pretty, 1));
    }

    // warnings
    detail::JsonArr warn_arr;
    for(const auto& w : sys.warnings)
    {
        warn_arr.add(json_str(w));
    }
    root.add("warnings", warn_arr.build(pretty, 1));

    // raw：仅当 System::raw 有值且 include_raw 为 true 时输出
    if(opts.include_raw && sys.raw)
    {
        root.add("raw", raw_store_to_system_json(*sys.raw, pretty, 1));
    }

    return root.build(pretty, 0);
}

System from_json(std::string_view json)
{
    detail::JsonVal root;
    try
    {
        root = detail::parse_json(json);
    }
    catch(const detail::JsonError& e)
    {
        throw SysalError(ErrorKind::DeserializationError, e.what());
    }

    if(root.type != detail::JsonVal::Type::Obj)
    {
        throw SysalError(ErrorKind::DeserializationError, "JSON root must be an object");
    }

    // 版本兼容性检查
    const auto* meta_val = root.get("meta");
    if(meta_val && meta_val->type == detail::JsonVal::Type::Obj)
    {
        const auto* ver_val = meta_val->get("sysal_version");
        if(ver_val && ver_val->type == detail::JsonVal::Type::Str)
        {
            if(!ver_val->str_val.starts_with("0.0."))
            {
                throw SysalError(ErrorKind::DeserializationError,
                                 "incompatible version: " + ver_val->str_val);
            }
        }
    }

    System sys;

    // info
    sys.info = system_info_from_json(req_obj(root, "info"));

    // meta
    if(meta_val && meta_val->type == detail::JsonVal::Type::Obj)
    {
        sys.meta = meta_from_json(*meta_val);
    }

    // warnings
    const auto* warn_val = root.get("warnings");
    if(warn_val && warn_val->type == detail::JsonVal::Type::Arr)
    {
        sys.warnings = str_array_from_json(*warn_val);
    }

    // raw
    const auto* raw_val = root.get("raw");
    if(raw_val && raw_val->type == detail::JsonVal::Type::Obj)
    {
        sys.raw = sys_raw_store_from_json(*raw_val);
    }

    return sys;
}

} // namespace sysal
