# 测试策略：Raw Replay

Raw-first 架构的核心收益：无需真实硬件即可测试 Parser/Resolver。

## 接口

```cpp
// include/sysal/test/replay.hpp

namespace sysal::test
{

// 从文件加载原始数据（失败时抛 SysalError）
RawStore load_raw_store(const std::string& path);

// 基于原始数据回放采集（失败时抛 SysalError）
System collect_from_raw(const RawStore& raw, Collect flags = Collect::full);

// 保存原始数据到文件（失败时抛 SysalError）
void save_raw_store(const RawStore& raw, const std::string& path);

}  // namespace sysal::test
```

## 工作流

```cpp
// 1. 采集（一次性，在真实硬件上）：
auto sys = sysal::System::collect(sysal::Collect::full | sysal::Collect::Raw);
sysal::test::save_raw_store(*sys.raw, "tests/fixtures/gpu_server_8xH20.json");

// 2. 回放（CI / 开发机，无需硬件）：
auto raw = sysal::test::load_raw_store("tests/fixtures/gpu_server_8xH20.json");
auto sys = sysal::test::collect_from_raw(raw, sysal::Collect::full);

// 3. 断言：
assert(sys.info.accelerators.gpus().size() == 8);
```

## 管线对比

```txt
正常：Reader → RawStore → Parser → ParseResult → Resolver → System
回放：            RawStore → Parser → ParseResult → Resolver → System
```

Reader 被跳过；Parser 和 Resolver 被完整执行。

## Fixture 布局

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
│   └── test_network.cpp
```

## 错误处理

所有接口失败时抛出 `SysalError`，不返回 `Expected`。
