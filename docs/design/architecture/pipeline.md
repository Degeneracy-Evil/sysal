# 内部管线与模块结构

## 管线

```txt
Reader → RawStore → Parser → ParseResult → Resolver → System
```

- **Reader**：将原始信息收集到 `RawStore`。
- **Parser**：将 `RawStore` 转换为按域划分的 `ParseResult`（无跨域引用，直接使用公共类型）。
- **Resolver**：合并 `ParseResult`，解决冲突，计算可见性，组装 `System` 对象。

## ParseResult（内部契约）

```cpp
namespace sysal::detail
{

struct ParseResult
{
    std::optional<PlatformInfo>          platform;
    std::optional<CpuSubsystem>          cpu;
    std::optional<MemorySubsystem>       memory;
    std::optional<PciSubsystem>          pci;
    std::optional<NetworkSubsystem>      network;
    std::optional<AcceleratorSubsystem>  accelerators;
    std::optional<StorageSubsystem>      storage;
    std::optional<SoftwareStackInfo>     software;
    std::optional<ExecutionContextInfo>  execution;
};

}  // namespace sysal::detail
```

`ParseResult` 位于 `src/parser/parse_result.hpp`（内部，不在 `include/sysal/` 中）。
每个字段都是 `optional` —— 该域可能失败或未被请求。
各字段直接使用公共 API 中定义的类型，不再使用私有 `*Facts` 结构。

## 源码布局

```txt
sysal/
├── include/sysal/
│   ├── sysal.hpp
│   ├── system.hpp               # System 类 + SystemInfo
│   ├── collect.hpp              # Collect 位掩码枚举
│   ├── snapshot_meta.hpp
│   ├── platform_info.hpp
│   ├── resource_info.hpp
│   ├── software_stack_info.hpp
│   ├── execution_context_info.hpp
│   ├── raw_store.hpp
│   ├── error.hpp
│   ├── enums.hpp
│   ├── ids.hpp
│   ├── units.hpp
│   ├── value_types.hpp
│   ├── strong_id.hpp
│   ├── serialization.hpp        # 可选
│   └── test/replay.hpp          # 测试工具
│
└── src/
    ├── api/                     # 公共 API 实现
    │   └── system.cpp           # System::collect() / refresh()
    │
    ├── model/                   # 数据模型实现
    │   ├── raw_store.cpp        # RawStore 方法
    │   └── resource_info.cpp    # ResourceInfo 便利查询方法
    │
    ├── reader/linux/            # 平台相关 Reader
    │   ├── procfs.hpp / procfs.cpp
    │   ├── sysfs.hpp / sysfs.cpp
    │   └── file_utils.hpp
    │
    ├── parser/                  # 原始数据 → 结构化事实
    │   ├── parse_utils.hpp
    │   ├── parse_result.hpp     # ParseResult 定义
    │   ├── cpu.hpp / cpu.cpp
    │   ├── memory.hpp / memory.cpp
    │   ├── pci.hpp / pci.cpp
    │   ├── network.hpp / network.cpp
    │   ├── accelerator.hpp / accelerator.cpp
    │   ├── storage.hpp / storage.cpp
    │   ├── software.hpp / software.cpp
    │   └── execution.hpp / execution.cpp
    │
    ├── resolver/                # 结构化事实 → 最终快照
    │   └── resolve.hpp / resolve.cpp
    │
    ├── serialization/           # JSON 序列化
    │   ├── json.hpp              # 手写 JSON 引擎
    │   └── serialize.cpp         # System ↔ JSON
    │
    └── pipeline/                # 流程编排
        └── pipeline.hpp / pipeline.cpp
```

### 目录职责

| 目录 | 职责 |
|---|---|
| `api/` | `System::collect()` / `refresh()` 实现，公共入口 |
| `model/` | `RawStore`、`ResourceInfo` 等数据模型的方法实现 |
| `reader/linux/` | 平台相关的原始数据采集（procfs / sysfs） |
| `parser/` | 从 `RawStore` 解析出 `ParseResult`（按域独立） |
| `resolver/` | 从 `ParseResult` 组装 `System`（可见性、冲突解决） |
| `serialization/` | JSON 序列化引擎与 `System` 的序列化实现 |
| `pipeline/` | 编排 Reader → Parser → Resolver 的完整流程 |

平台相关的 reader 位于 `src/reader/<platform>/` 下。
xmake 在构建时选择平台目录。
