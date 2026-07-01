# 测试策略：Raw Replay

Raw-first 架构的核心收益：无需真实硬件即可测试 Parser / Resolver。

## 接口

```cpp
// include/sysal/test/replay.hpp

namespace sysal::test
{

// 从 JSON 文件加载原始数据（失败时抛 SysalError）
RawStore load_raw_store(const std::string& path);

// 基于原始数据回放采集，跳过 Reader 阶段（失败时抛 SysalError）
System collect_from_raw(const RawStore& raw, Collect flags = Collect::full);

// 将原始数据保存到 JSON 文件（失败时抛 SysalError）
void save_raw_store(const RawStore& raw, const std::string& path);

}  // namespace sysal::test
```

## 工作流

```cpp
// 1. 采集（一次性，在真实硬件上）：
auto sys = sysal::System::collect(sysal::Collect::full | sysal::Collect::Raw);
// 前置条件：sys.raw 必须有值（即请求中包含 Collect::Raw）
assert(sys.raw.has_value());
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

## `collect_from_raw` 的后端行为

回放模式下不需要后端初始化（NVML `nvmlInit` 等），因为：
- 原始数据（如 `nvidia-smi` 输出、NVML 查询结果）已在采集阶段保存到 `RawStore` 中。
- Parser 直接从 `RawStore` 解析，不接触真实硬件。
- Resolver 只做组装和可见性计算，不发起硬件查询。

因此 `collect_from_raw` 可在无 GPU、无 NVML 驱动的 CI 环境中运行。

## Fixture 文件格式

Fixture 文件就是 `save_raw_store` 输出的 JSON，格式为 `RawStore` 的序列化结果
（非 `System` 的完整序列化）。与 `serialization.hpp` 的 `to_json` 输出不同——
`to_json` 序列化整个 `System`，而 fixture 只包含 `RawStore` 部分。

## 目标 Fixture 布局

```txt
tests/
├── fixtures/
│   ├── cpu_only_192cpu.json
│   ├── gpu_server_8xH20.json
│   ├── container_docker.json
│   └── numa_8node.json
├── test_replay.cpp               # 当前回放测试入口
└── examples/sysal_info.cpp       # 全量 API 演示
```

v0.0.1 使用单个 `test_replay.cpp` 作为回放测试入口，后续可按子域拆分。

## 错误处理

所有接口失败时抛出 `SysalError`，不返回 `Expected`。
