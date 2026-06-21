# Internal Pipeline & Module Structure

## Pipeline

```txt
Reader → RawStore → Parser → ParsedFacts → Resolver → SystemSnapshot
```

- **Reader**: collects raw evidence into `RawStore`.
- **Parser**: converts `RawStore` into per-domain `ParsedFacts` (no cross-domain references).
- **Resolver**: merges `ParsedFacts`, resolves conflicts, computes visibility, builds topology, assembles `SystemSnapshot`.

## ParsedFacts (internal contract)

```cpp
namespace sysal::detail
{

struct CpuFacts         { /* packages, cores, logical_cpus, numa_nodes, ... */ };
struct MemoryFacts      { /* total, available, numa_memory */ };
struct PciFacts         { /* devices */ };
struct NetworkFacts     { /* interfaces */ };
struct AcceleratorFacts { /* devices */ };
struct StorageFacts     { /* devices */ };
struct PlatformFacts    { /* host, os, kernel, arch, ... */ };
struct SoftwareFacts    { /* drivers, runtimes, cuda, rocm, ... */ };
struct ExecutionFacts   { /* process, env, cgroup, cpuset, ... */ };
struct TopologyFacts    { /* numa_relations, pci_relations, device_localities */ };

struct ParsedFacts
{
    std::optional<PlatformFacts>    platform;
    std::optional<CpuFacts>         cpu;
    std::optional<MemoryFacts>      memory;
    std::optional<PciFacts>         pci;
    std::optional<NetworkFacts>     network;
    std::optional<AcceleratorFacts> accelerators;
    std::optional<StorageFacts>     storage;
    std::optional<SoftwareFacts>    software;
    std::optional<ExecutionFacts>   execution;
    std::optional<TopologyFacts>    topology;
};

}  // namespace sysal::detail
```

`ParsedFacts` lives in `src/parser/parsed_facts.hpp` (internal, not in `include/sysal/`).
Each field is `optional` — the domain may have failed or was not requested.

## Source Layout

```txt
sysal/
├── include/sysal/
│   ├── sysal.hpp
│   ├── collect_spec.hpp
│   ├── system_snapshot.hpp
│   ├── snapshot_meta.hpp
│   ├── platform_info.hpp
│   ├── resource_info.hpp
│   ├── software_stack_info.hpp
│   ├── execution_context_info.hpp
│   ├── topology_info.hpp
│   ├── raw_store.hpp
│   ├── diagnostics.hpp
│   ├── error.hpp
│   ├── serialization.hpp          (optional)
│   └── test/replay.hpp             (test utility)
│
└── src/
    ├── public_api/
    │   └── collect.cpp
    │
    ├── raw/
    │   └── raw_store.cpp
    │
    ├── reader/
    │   └── linux/
    │       ├── procfs_reader.cpp
    │       ├── sysfs_reader.cpp
    │       ├── command_reader.cpp
    │       ├── hwloc_reader.cpp
    │       ├── nvml_reader.cpp
    │       └── ibverbs_reader.cpp
    │
    ├── parser/
    │   ├── parsed_facts.hpp        (internal)
    │   ├── cpu_parser.cpp
    │   ├── memory_parser.cpp
    │   ├── pci_parser.cpp
    │   ├── network_parser.cpp
    │   ├── accelerator_parser.cpp
    │   └── software_parser.cpp
    │
    ├── resolver/
    │   ├── resource_resolver.cpp
    │   ├── topology_resolver.cpp
    │   ├── visibility_resolver.cpp
    │   └── software_stack_resolver.cpp
    │
    └── backend/
        ├── hwloc_backend.cpp
        ├── nvml_backend.cpp
        └── ibverbs_backend.cpp
```

Platform-specific readers live under `src/reader/<platform>/`.
xmake.lua selects the platform directory at build time.
