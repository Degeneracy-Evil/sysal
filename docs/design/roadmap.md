# Roadmap

## v0.0.1 Implementation Scope

Required:

```txt
Public API:
    collect(spec) -> Expected<SystemSnapshot, SysalError>
    collect_or_throw(spec) -> SystemSnapshot
    CollectSpec builder + presets

Core model:
    SystemSnapshot (with SnapshotMeta)
    PlatformInfo
    ResourceInfo (CPU, Memory, Accelerator, Network, PCI, Storage, Topology)
    SoftwareStackInfo
    ExecutionContextInfo (with visibility indexes)
    Diagnostics (with ConflictDetail)
    RawStore (multi-record)

Internal pipeline:
    Reader → RawStore → Parser → ParsedFacts → Resolver → SystemSnapshot

Linux support:
    procfs, sysfs, PCI, basic network, basic CPU and memory

Optional backend:
    hwloc for topology
    NVML for NVIDIA GPU

Testing:
    collect_from_raw + load/save_raw_store for raw replay

Conflict resolution:
    Source trust order + per-category rules

Thread safety:
    Stateless collect, immutable snapshot
```

Recommended first implementation targets:

```txt
CPU:        architecture, packages, cores, logical CPUs (with parent IDs),
            ISA extensions, visible CPUs
Memory:     total, NUMA memory
Accelerator: NVIDIA GPU name, memory, PCI address, visibility
Network:    name, state, speed, IP, PCI address, visibility
Software:   OS, kernel, NVIDIA driver, CUDA runtime
Execution:  env vars, cgroup/cpuset visibility, container detection
Raw:        optional raw records
Diagnostics: warnings, errors, conflict details
```

## v0.0.1 Non-Goals

```txt
Performance scoring
Benchmark execution
Operator selection
Scheduling policy
Long-term monitoring
Daemon mode
Database storage
Web API
Complex plugin system
Full cross-platform support
Complete hardware inventory
Caching / incremental collection
```

## Future Extensions

### Caching (post-v0.0.1)

v0.0.1 uses a stateless function API. Future caching via a `Collector` class
preserves backward compatibility:

```cpp
class Collector
{
public:
    explicit Collector(const CollectSpec& default_spec = {});
    SystemSnapshot collect(const CollectSpec& spec = {});
    SystemSnapshot collect_or_throw(const CollectSpec& spec = {});
    // future:
    SystemSnapshot collect_incremental(const CollectSpec& spec = {});
    void invalidate_cache();
};
```

The free function `sysal::collect()` remains unchanged; it creates a temporary `Collector` internally.

### Cross-Platform (post-v0.0.1)

v0.0.1 is Linux-only. Extension path:

1. `RawSource` enum gains platform-specific values (e.g. `WmiCpu`, `SysctlHw`).
2. Readers live under `src/reader/<platform>/`, selected by xmake at build time.
3. No public API changes needed — `SystemSnapshot` is platform-agnostic.
