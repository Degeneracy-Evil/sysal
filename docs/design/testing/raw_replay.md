# Testing Strategy: Raw Replay

Raw-first architecture's key benefit: test Parser/Resolver without real hardware.

## Interface

```cpp
// include/sysal/test/replay.hpp

namespace sysal::test
{

Expected<RawStore, SysalError> load_raw_store(const std::string& path);

Expected<SystemSnapshot, SysalError> collect_from_raw(const RawStore& raw,
                                                       const CollectSpec& spec = {});

Expected<void, SysalError> save_raw_store(const RawStore& raw,
                                          const std::string& path);

}  // namespace sysal::test
```

## Workflow

```txt
1. Capture (one-time, on real hardware):
   auto snapshot = sysal::collect(sysal::CollectSpec::full().with_raw());
   sysal::test::save_raw_store(*snapshot->raw(), "tests/fixtures/gpu_server_8xH20.json");

2. Replay (CI / dev machine, no hardware needed):
   auto raw = sysal::test::load_raw_store("tests/fixtures/gpu_server_8xH20.json");
   auto snapshot = sysal::test::collect_from_raw(*raw, sysal::CollectSpec::full());

3. Assert:
   assert(snapshot->resources.accelerators.gpus().size() == 8);
```

## Pipeline Comparison

```txt
Normal:    Reader → RawStore → Parser → ParsedFacts → Resolver → SystemSnapshot
Replay:                RawStore → Parser → ParsedFacts → Resolver → SystemSnapshot
```

Reader is skipped; Parser and Resolver are fully exercised.

## Fixture Layout

```txt
tests/
├── fixtures/
│   ├── cpu_only_192cpu.json
│   ├── gpu_server_8xH20.json
│   ├── container_docker.json
│   └── numa_8node.json
├── replay/
│   ├── test_cpu.cpp
│   ├── test_gpu.cpp
│   └── test_topology.cpp
```
