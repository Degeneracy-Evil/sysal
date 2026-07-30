/// @file test_serialization.cpp
/// @brief System JSON 序列化往返测试

#include "sysal/core/error.hpp"
#include "sysal/core/system.hpp"
#include "sysal/serialization/serialization.hpp"
#include "sysal/version.hpp"

#include "test_macros.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

using namespace sysal;

namespace
{

/// @brief 检查 JSON 字符串是否包含指定键
/// @param json JSON 文本
/// @param key 要查找的键名
/// @return 包含返回 true
bool json_contains_key(std::string_view json, std::string_view key)
{
    // 简单搜索 "key" 模式（带引号的键名）
    std::string pattern = "\"";
    pattern += key;
    pattern += "\"";
    return json.find(pattern) != std::string_view::npos;
}

void test_round_trip()
{
    std::cout << "  test_round_trip...\n";

    auto sys =
        System::collect(Collect::Platform | Collect::Cpu | Collect::Memory | Collect::Accelerator |
                        Collect::Network | Collect::Storage | Collect::Pci | Collect::Execution);

    // 序列化
    std::string json_str = to_json(sys, {.pretty_print = true});
    CHECK(!json_str.empty());

    // 反序列化
    System round_trip = from_json(json_str);

    // 比较关键字段
    CHECK(round_trip.info.cpu.logical_cpus.size() == sys.info.cpu.logical_cpus.size());
    CHECK(round_trip.info.memory.total_memory.value == sys.info.memory.total_memory.value);
    CHECK(round_trip.info.platform.os.name == sys.info.platform.os.name);
    CHECK(round_trip.info.platform.os.version == sys.info.platform.os.version);
    CHECK(round_trip.info.platform.host.hostname == sys.info.platform.host.hostname);
    CHECK(round_trip.info.platform.virtualization.has_value() ==
          sys.info.platform.virtualization.has_value());
    if(sys.info.platform.virtualization.has_value())
    {
        CHECK(round_trip.info.platform.virtualization->kind ==
              sys.info.platform.virtualization->kind);
        CHECK(round_trip.info.platform.virtualization->hypervisor ==
              sys.info.platform.virtualization->hypervisor);
    }
    CHECK(round_trip.info.execution.process.pid == sys.info.execution.process.pid);

    // Memory: v0.0.4 DIMM 字段
    CHECK(round_trip.info.memory.dimms.size() == sys.info.memory.dimms.size());
    CHECK(round_trip.info.memory.dimm_count.has_value() == sys.info.memory.dimm_count.has_value());
    if(sys.info.memory.dimm_count.has_value())
    {
        CHECK(round_trip.info.memory.dimm_count.value() > 0);
        CHECK(round_trip.info.memory.dimm_count.value() == sys.info.memory.dimm_count.value());
    }
    CHECK(round_trip.info.memory.populated_dimms.has_value() ==
          sys.info.memory.populated_dimms.has_value());
    if(sys.info.memory.populated_dimms.has_value())
    {
        CHECK(round_trip.info.memory.populated_dimms.value() > 0);
        CHECK(round_trip.info.memory.populated_dimms.value() ==
              sys.info.memory.populated_dimms.value());
    }
    CHECK(round_trip.info.memory.total_memory.value == sys.info.memory.total_memory.value);
    CHECK(round_trip.info.memory.memory_type == sys.info.memory.memory_type);
    CHECK(round_trip.info.memory.configured_speed_mts.has_value() ==
          sys.info.memory.configured_speed_mts.has_value());
    if(sys.info.memory.configured_speed_mts.has_value())
    {
        CHECK(round_trip.info.memory.configured_speed_mts->value ==
              sys.info.memory.configured_speed_mts->value);
    }
    if(!sys.info.memory.dimms.empty())
    {
        const auto& a = sys.info.memory.dimms[0];
        const auto& b = round_trip.info.memory.dimms[0];
        CHECK(b.size.value == a.size.value);
        CHECK(b.speed_mts.has_value() == a.speed_mts.has_value());
        if(a.speed_mts.has_value())
        {
            CHECK(b.speed_mts->value == a.speed_mts->value);
        }
        CHECK(b.manufacturer.has_value() == a.manufacturer.has_value());
        if(a.manufacturer.has_value())
        {
            CHECK(b.manufacturer->value == a.manufacturer->value);
        }
    }

    // Accelerator
    CHECK(round_trip.info.accelerators.devices.size() == sys.info.accelerators.devices.size());
    if(!sys.info.accelerators.devices.empty())
    {
        const auto& a = sys.info.accelerators.devices[0];
        const auto& b = round_trip.info.accelerators.devices[0];
        CHECK(b.name.value == a.name.value);
        CHECK(b.kind == a.kind);
        CHECK(b.memory_size.has_value() == a.memory_size.has_value());
        if(a.memory_size.has_value())
        {
            CHECK(b.memory_size->value == a.memory_size->value);
        }
    }

    // Storage
    CHECK(round_trip.info.storage.devices.size() == sys.info.storage.devices.size());
    if(!sys.info.storage.devices.empty())
    {
        const auto& a = sys.info.storage.devices[0];
        const auto& b = round_trip.info.storage.devices[0];
        CHECK(b.name.value == a.name.value);
        CHECK(b.kind == a.kind);
        CHECK(b.capacity.has_value() == a.capacity.has_value());
        if(a.capacity.has_value())
        {
            CHECK(b.capacity->value == a.capacity->value);
        }
    }

    // Storage: v0.0.4 mount_point / fs_type 往返
    for(std::size_t i = 0; i < sys.info.storage.devices.size(); ++i)
    {
        const auto& a = sys.info.storage.devices[i];
        if(a.mount_point.has_value())
        {
            const auto& b = round_trip.info.storage.devices[i];
            CHECK(b.mount_point.has_value());
            CHECK(b.mount_point->value == a.mount_point->value);
            CHECK(b.fs_type.has_value() == a.fs_type.has_value());
            if(a.fs_type.has_value())
            {
                CHECK(b.fs_type->value == a.fs_type->value);
            }
            break;
        }
    }

    // PCI
    CHECK(round_trip.info.pci.devices.size() == sys.info.pci.devices.size());
    if(!sys.info.pci.devices.empty())
    {
        const auto& a = sys.info.pci.devices[0];
        const auto& b = round_trip.info.pci.devices[0];
        CHECK(b.address.domain == a.address.domain);
        CHECK(b.address.bus == a.address.bus);
        CHECK(b.vendor.value == a.vendor.value);
    }

    // Network
    CHECK(round_trip.info.network.interfaces.size() == sys.info.network.interfaces.size());
    if(!sys.info.network.interfaces.empty())
    {
        const auto& a = sys.info.network.interfaces[0];
        const auto& b = round_trip.info.network.interfaces[0];
        CHECK(b.name.value == a.name.value);
        CHECK(b.state == a.state);
    }

    // Network: v0.0.4 addresses 往返
    for(std::size_t i = 0; i < sys.info.network.interfaces.size(); ++i)
    {
        const auto& a = sys.info.network.interfaces[i];
        if(!a.addresses.empty())
        {
            const auto& b = round_trip.info.network.interfaces[i];
            CHECK(b.addresses.size() == a.addresses.size());
            for(std::size_t j = 0; j < a.addresses.size(); ++j)
            {
                CHECK(b.addresses[j].value == a.addresses[j].value);
            }
            break;
        }
    }

    // Network: v0.0.4 pci_address 往返
    for(std::size_t i = 0; i < sys.info.network.interfaces.size(); ++i)
    {
        const auto& a = sys.info.network.interfaces[i];
        if(a.pci_address.has_value())
        {
            const auto& b = round_trip.info.network.interfaces[i];
            CHECK(b.pci_address.has_value());
            CHECK(b.pci_address->domain == a.pci_address->domain);
            CHECK(b.pci_address->bus == a.pci_address->bus);
            CHECK(b.pci_address->device == a.pci_address->device);
            CHECK(b.pci_address->function == a.pci_address->function);
            break;
        }
    }

    // Platform: kernel 与架构
    CHECK(round_trip.info.platform.kernel.release == sys.info.platform.kernel.release);
    CHECK(round_trip.info.platform.architecture.name == sys.info.platform.architecture.name);
    CHECK(round_trip.info.platform.architecture.bits == sys.info.platform.architecture.bits);

    // Meta
    CHECK(round_trip.meta.collect_duration.count() >= 0.0);
    CHECK(sys.meta.collect_duration.count() >= 0.0);
    CHECK(round_trip.meta.requested_flags == sys.meta.requested_flags);
}

void test_no_raw_when_excluded()
{
    std::cout << "  test_no_raw_when_excluded...\n";

    auto sys = System::collect(Collect::Cpu | Collect::Memory | Collect::Raw);

    // include_raw = false → 不应包含 "raw" 键
    std::string json_str = to_json(sys, {.pretty_print = false, .include_raw = false});
    CHECK(!json_contains_key(json_str, "raw"));
}

void test_raw_when_included()
{
    std::cout << "  test_raw_when_included...\n";

    auto sys = System::collect(Collect::Cpu | Collect::Memory | Collect::Raw);

    // include_raw = true 且 raw 有值 → 应包含 "raw" 键
    if(sys.raw)
    {
        std::string json_str = to_json(sys, {.pretty_print = false, .include_raw = true});
        CHECK(json_contains_key(json_str, "raw"));
    }
    else
    {
        // raw 为空时即使 include_raw=true 也不输出
        CHECK(true);
    }
}

void test_no_meta_when_excluded()
{
    std::cout << "  test_no_meta_when_excluded...\n";

    auto sys = System::collect(Collect::Cpu | Collect::Memory);

    // include_meta = false → 不应包含 "meta" 键
    std::string json_str = to_json(sys, {.pretty_print = false, .include_meta = false});
    CHECK(!json_contains_key(json_str, "meta"));
}

void test_meta_when_included()
{
    std::cout << "  test_meta_when_included...\n";

    auto sys = System::collect(Collect::Cpu | Collect::Memory);

    // include_meta = true → 应包含 "meta" 键
    std::string json_str = to_json(sys, {.pretty_print = false, .include_meta = true});
    CHECK(json_contains_key(json_str, "meta"));
}

void test_version_mismatch()
{
    std::cout << "  test_version_mismatch...\n";

    // 构造一个不兼容版本的 JSON
    std::string bad_json = R"({
    "info": {
        "platform": {
            "host": {"hostname":"h","machine_id":"m","product_name":"p","vendor":"v","serial":"s"},
            "os": {"name":"n","version":"v","distribution":"d","distribution_version":"dv","codename":"c"},
            "kernel": {"release":"r","version":"v","compiled_at":"c","architecture":"a"},
            "architecture": {"name":"x86_64","bits":64,"byte_order":"little"}
        },
        "cpu": {"arch":0,"packages":[],"cores":[],"logical_cpus":[],"numa_nodes":[],"isa_extensions":[]},
        "memory": {"total_memory":0,"numa_memory":[]},
        "accelerators": {"devices":[]},
        "network": {"interfaces":[]},
        "storage": {"devices":[]},
        "pci": {"devices":[]},
        "software": {"drivers":[],"runtimes":[],"compilers":[],"libraries":[]},
        "execution": {
            "process": {"pid":1,"ppid":0,"uid":0,"gid":0,"comm":"t","exe":"","cwd":""},
            "environment": {"entries":[]},
            "cgroup": {"version":0,"path":"","controllers":[]},
            "cpuset": {"cpus":"","mems":"","cpus_effective":"","mems_effective":""},
            "permission": {"euid":0,"egid":0,"capabilities":[],"is_root":false},
            "visible_logical_cpu_ids":[],
            "visible_accelerator_ids":[],
            "visible_network_interface_names":[]
        }
    },
    "meta": {
        "collect_time": 0,
        "sysal_version": "99.0.0",
        "collect_duration": 0.0,
        "requested_flags": 0,
        "succeeded_collectors": [],
        "failed_collectors": []
    },
    "warnings": []
})";

    bool threw = false;
    try
    {
        (void)from_json(bad_json);
    }
    catch(const SysalError& e)
    {
        threw = (e.kind() == ErrorKind::DeserializationError);
    }
    CHECK(threw);
}

void test_compatible_version()
{
    std::cout << "  test_compatible_version...\n";

    // 构造一个兼容版本 (0.0.x) 的 JSON
    std::string good_json = R"({
    "info": {
        "platform": {
            "host": {"hostname":"h","machine_id":"m","product_name":"p","vendor":"v","serial":"s"},
            "os": {"name":"n","version":"v","distribution":"d","distribution_version":"dv","codename":"c"},
            "kernel": {"release":"r","version":"v","compiled_at":"c","architecture":"a"},
            "architecture": {"name":"x86_64","bits":64,"byte_order":"little"}
        },
        "cpu": {"arch":0,"packages":[],"cores":[],"logical_cpus":[],"numa_nodes":[],"isa_extensions":[]},
        "memory": {"total_memory":0,"numa_memory":[]},
        "accelerators": {"devices":[]},
        "network": {"interfaces":[]},
        "storage": {"devices":[]},
        "pci": {"devices":[]},
        "software": {"drivers":[],"runtimes":[],"compilers":[],"libraries":[]},
        "execution": {
            "process": {"pid":1,"ppid":0,"uid":0,"gid":0,"comm":"t","exe":"","cwd":""},
            "environment": {"entries":[]},
            "cgroup": {"version":0,"path":"","controllers":[]},
            "cpuset": {"cpus":"","mems":"","cpus_effective":"","mems_effective":""},
            "permission": {"euid":0,"egid":0,"capabilities":[],"is_root":false},
            "visible_logical_cpu_ids":[],
            "visible_accelerator_ids":[],
            "visible_network_interface_names":[]
        }
    },
    "meta": {
        "collect_time": 0,
        "sysal_version": "0.0.5",
        "collect_duration": 0.0,
        "requested_flags": 0,
        "succeeded_collectors": [],
        "failed_collectors": []
    },
    "warnings": []
})";

    System sys = from_json(good_json);
    CHECK(sys.info.platform.host.hostname == "h");
    CHECK(sys.meta.sysal_version == sysal::VERSION_STRING);
}

} // namespace

int main()
{
    std::cout << "test_serialization:\n";

    test_round_trip();
    test_no_raw_when_excluded();
    test_raw_when_included();
    test_no_meta_when_excluded();
    test_meta_when_included();
    test_version_mismatch();
    test_compatible_version();

    TEST_SUMMARY();
}
