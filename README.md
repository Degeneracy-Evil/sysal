# sysal

sysal is a C++ system information abstraction library for servers.

It is designed to collect, normalize, and expose system information through a clean typed API. sysal focuses on returning real system facts, including hardware resources, software stack, runtime environment, network devices, PCI topology, accelerator information, and raw evidence when needed.

sysal is a **library**, not a standalone system inspection tool.

## Project Goal

sysal aims to provide a stable and extensible system information layer for upper-level projects.

It answers questions such as:

* What CPU, memory, GPU, NIC, storage, and PCI resources does this server have?
* What resources are visible to the current process?
* What software stack is available, such as CUDA, ROCm, MPI, UCX, or BLAS libraries?
* How are devices connected through NUMA and PCI topology?
* What raw system evidence was used to build the final typed model?

sysal only returns facts. It does not make scheduling, benchmarking, or performance decisions.

## Non-Goals

sysal is not:

* an operator scheduler
* a benchmark framework
* a performance scoring system
* a daemon
* a monitoring system
* a web service
* a replacement for hwloc or other system libraries

External libraries such as `hwloc`, `NVML`, or `ibverbs` may be used as backends, but sysal keeps its own public data model.

## Core Concept

sysal follows this internal pipeline:

```txt
Read / Probe
    ↓
RawStore
    ↓
Parser / Adapter
    ↓
Normalized Facts
    ↓
Resolver / Topology Builder
    ↓
SystemSnapshot
```

The design principle is:

```txt
Raw evidence first.
Typed model second.
Decision never.
```

## Main API Style

The public API should remain small and clear.

Example:

```cpp
auto snapshot = sysal::collect_or_throw(
    sysal::CollectSpec::for_operator_dispatch()
        .with_raw()
);

const auto& resources = snapshot.resources;

const auto& cpu = resources.cpu;
const auto& memory = resources.memory;
const auto& accelerators = resources.accelerators;
const auto& network = resources.network;
```

Non-throwing style:

```cpp
auto result = sysal::collect(
    sysal::CollectSpec::for_operator_dispatch()
        .with_raw()
);

if (!result) {
    return report_error(result.error());
}

const auto& snapshot = *result;
const auto& resources = snapshot.resources;
```

## Main Data Model

The central object is `SystemSnapshot`.

```cpp
struct SystemSnapshot {
    SnapshotMeta meta;
    PlatformInfo platform;
    ResourceInfo resources;
    SoftwareStackInfo software;
    ExecutionContextInfo execution;
    Diagnostics diagnostics;
    std::optional<RawStore> raw;
};
```

Main sections:

* `PlatformInfo`: host, OS, kernel, architecture, firmware, virtualization
* `ResourceInfo`: CPU, memory, accelerators, network, storage, PCI, topology
* `SoftwareStackInfo`: drivers, runtimes, compilers, libraries, CUDA, ROCm, MPI, RDMA stack
* `ExecutionContextInfo`: process environment, cgroup, cpuset, permissions, container visibility
* `TopologyInfo`: NUMA, PCI, device locality, GPU/NIC relationships
* `RawStore`: optional raw records from `/proc`, `/sys`, `hwloc`, `lspci`, `nvidia-smi`, `ibverbs`, etc.
* `Diagnostics`: warnings and errors during collection, parsing, and resolving

## Design Principles

### Strong Typed Model

sysal avoids using generic string maps for structured information.

Prefer:

```cpp
struct MemorySize {
    std::uint64_t bytes;
};

struct Frequency {
    std::uint64_t hz;
};

struct PciAddress {
    std::uint16_t domain;
    std::uint8_t bus;
    std::uint8_t device;
    std::uint8_t function;
};
```

Instead of:

```cpp
std::unordered_map<std::string, std::string> info;
```

This improves type safety, LSP completion, maintainability, and API discoverability.

### Raw Evidence Support

sysal keeps a clear distinction between:

```txt
Raw evidence
    and
Normalized typed model
```

Raw data is useful for debugging, testing, and verifying parser behavior.
The final public model should remain typed and structured.

### Backend Independence

sysal may use existing system libraries internally.

Recommended backend direction:

* topology: `hwloc`
* NVIDIA GPU: `NVML`
* RDMA: `ibverbs` and sysfs
* generic Linux hardware information: `/proc`, `/sys`, PCI, command output
* optional hardware inventory fallback: `hwinfo`

Backend-specific types should not leak into the public API.

## Development Environment

Current target development environment:

* OS: Ubuntu Server 24.04
* Language: C++23
* Build system: xmake
* Code formatting: clang-format
* Static analysis: clang-tidy

### Prerequisites

| Tool | Min Version | Description |
|------|-------------|-------------|
| clang | 17 | C++23 compiler |
| xmake | 2.8 | Build system |
| lld | — | Linker |
| libc++ | — | C++ standard library (bundled with clang) |
| clang-format | — | Code formatter (bundled with clang) |
| clang-tidy | — | Static analysis (bundled with clang) |
| perl | — | check.sh whitespace fix |

## Build

```bash
xmake
```

`compile_commands.json` is auto-generated to `build/` on build (for clang-tidy / clangd), no manual step needed.

## CI

On push to `main` or PR, GitHub Actions automatically runs `utils/check.sh` full checks.

## Run Tests

```bash
xmake test
```

## Format Code

```bash
xmake format
```

Or directly:

```bash
clang-format -i <files>
```

## Static Analysis

```bash
xmake check
```

Or directly with clang-tidy, depending on project configuration.

## Initial Version Scope

sysal v0.0.1 focuses on building a clean foundation.

Planned scope:

* public API: `collect(spec) -> SystemSnapshot`
* typed model for platform, resources, software stack, execution context, topology, raw records, and diagnostics
* Linux support through `/proc`, `/sys`, and PCI information
* basic CPU and memory collection
* basic network interface collection
* basic NVIDIA GPU collection when NVML is available
* optional hwloc-based topology collection
* raw record storage for debugging
* diagnostics for partial failures and fallback behavior

Out of scope for v0.0.1:

* performance prediction
* benchmark execution
* operator selection
* scheduling policy
* daemon mode
* database storage
* web API
* full cross-platform support
* complex plugin system

## Relationship With Other Projects

sysal is intended to serve as a system information provider.

```txt
sysal:
    What does this system have?
    What can the current process see?

opal:
    Which operator should be selected?

opbl:
    How fast does an operator run?
```

sysal should remain independent from opal and opbl.

## License

Apache License 2.0
