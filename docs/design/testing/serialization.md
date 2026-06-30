# 序列化

JSON 序列化通过独立头文件中的非侵入式自由函数实现：

```cpp
// include/sysal/serialization.hpp

namespace sysal
{

struct SerializationOptions
{
    bool pretty_print = false;
    bool include_raw = false;
    bool include_meta = true;
};

std::string to_json(const System& sys, const SerializationOptions& opts = {});

System from_json(std::string_view json);

}  // namespace sysal
```

## 设计要点

1. 非侵入式：自由函数，不在 `System` 上添加方法。
2. 独立头文件：`#include <sysal/serialization.hpp>` 按需引入。
3. `from_json` 支持 raw replay 测试。
4. `SnapshotMeta::sysal_version` 会写入 JSON；`from_json` 检查兼容性。
5. 使用手写 JSON 序列化（不依赖外部库）。
6. `to_json` 序列化 `System` 的全部公开成员（`info` / `meta` / `warnings` / `raw`）。

## 错误处理

`from_json` 失败时抛出 `SysalError`，不返回 `Expected`。
