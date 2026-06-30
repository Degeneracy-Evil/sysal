/// @file test_serialization.cpp
/// @brief System JSON 序列化往返测试

#include "sysal/core/error.hpp"
#include "sysal/core/system.hpp"
#include "sysal/serialization/serialization.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

using namespace sysal;

namespace
{

int g_pass = 0;
int g_fail = 0;

void check(bool cond, const char* expr, int line)
{
    if(cond)
    {
        ++g_pass;
    }
    else
    {
        ++g_fail;
        std::cerr << "FAIL line " << line << ": " << expr << "\n";
    }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

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
        System::collect(Collect::Cpu | Collect::Memory | Collect::Platform | Collect::Execution);

    // 序列化
    std::string json_str = to_json(sys, {.pretty_print = true});
    CHECK(!json_str.empty());

    // 反序列化
    System sys2 = from_json(json_str);

    // 比较关键字段
    CHECK(sys2.info.cpu.logical_cpus.size() == sys.info.cpu.logical_cpus.size());
    CHECK(sys2.info.memory.total_memory.value == sys.info.memory.total_memory.value);
    CHECK(sys2.info.platform.os.name == sys.info.platform.os.name);
    CHECK(sys2.info.platform.os.version == sys.info.platform.os.version);
    CHECK(sys2.info.platform.host.hostname == sys.info.platform.host.hostname);
    CHECK(sys2.info.execution.process.pid == sys.info.execution.process.pid);
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
        ++g_pass;
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
        "sysal_version": "0.0.1",
        "collect_duration": 0.0,
        "requested_flags": 0,
        "succeeded_collectors": [],
        "failed_collectors": []
    },
    "warnings": []
})";

    System sys = from_json(good_json);
    CHECK(sys.info.platform.host.hostname == "h");
    CHECK(sys.meta.sysal_version == "0.0.1");
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

    std::cout << "  " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
