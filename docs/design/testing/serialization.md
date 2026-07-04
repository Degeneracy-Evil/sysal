# 序列化

JSON 序列化通过独立头文件中的非侵入式自由函数实现。

## 接口

```cpp
// include/sysal/serialization/serialization.hpp

namespace sysal
{

struct SerializationOptions
{
    bool pretty_print = false;  // 是否美化输出（换行 + 缩进）
    bool include_raw  = false;  // 是否输出 raw 字段（仅当 System::raw 有值时有效）
    bool include_meta = true;   // 是否输出 meta 字段
};

// 将 System 序列化为 JSON 字符串
std::string to_json(const System& sys, const SerializationOptions& opts = {});

// 从 JSON 字符串反序列化为 System（失败时抛出 SysalError）
System from_json(std::string_view json);

}  // namespace sysal
```

## JSON 结构

```json
{
  "info": {
    "platform": { ... },
    "cpu": { ... },
    "memory": { ... },
    "accelerators": { ... },
    "network": { ... },
    "storage": { ... },
    "pci": { ... },
    "software": { ... },
    "execution": { ... }
  },
  "meta": {
    "collect_time": 1782036690146,
    "sysal_version": "0.0.3",
    "collect_duration": 0.158,
    "requested_flags": 1023,
    "succeeded_collectors": ["platform", "cpu", ...],
    "failed_collectors": []
  },
  "warnings": ["NVML is unavailable, GPU collection skipped."],
  "raw": { ... }
}
```

`raw` 字段仅当 `System::raw` 有值 **且** `include_raw = true` 时输出。
若 `System::raw` 为 `nullopt`，`include_raw` 的值无意义，`raw` 字段不输出。

## 设计要点

1. **非侵入式**：自由函数，不在 `System` 上添加方法。
2. **独立头文件**：`#include <sysal/serialization/serialization.hpp>` 按需引入，不强制依赖。
3. **`from_json` 的用途**：主要用于 raw replay 测试——从 JSON 还原 `System`，提取 `raw` 后走 `collect_from_raw` 回放管线。
4. **版本兼容**：`SnapshotMeta::sysal_version` 写入 JSON；`from_json` 检查版本兼容性，不兼容时抛出 `SysalError`。
5. **JSON 库**：序列化引擎使用 nlohmann/json 库（通过 xmake `add_requires("nlohmann_json")` 从 xrepo 管理依赖），序列化实现位于 `src/serialization/serialize.cpp`。

## 错误处理

`from_json` 失败时抛出 `SysalError`，不返回 `Expected`。
失败原因包括 JSON 语法错误、版本不兼容、必填字段缺失等。
