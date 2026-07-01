# 版本固定与 GitHub Release 计划

## 现状

版本号 `"0.0.1"` 硬编码在 `pipeline.cpp:152`，无集中管理。3 个测试文件断言精确字符串。序列化兼容性检查硬编码 `"0.0."` 前缀。

## 改动范围

### Phase V1: 创建版本头文件

**新建** `include/sysal/version.hpp`:

```cpp
/// @file version.hpp
/// @brief sysal 版本定义
/// @details 集中管理 sysal 版本号，供运行时元数据、序列化兼容性检查和构建系统引用。

#pragma once

namespace sysal
{

/// @brief 主版本号
inline constexpr int VERSION_MAJOR = 0;

/// @brief 次版本号
inline constexpr int VERSION_MINOR = 0;

/// @brief 修订版本号
inline constexpr int VERSION_PATCH = 1;

/// @brief 完整版本字符串（如 "0.0.1"）
inline constexpr const char* VERSION_STRING = "0.0.1";

} // namespace sysal
```

用 `inline constexpr` 而非 `#define`——类型安全，且可被 `constexpr` 上下文使用。

### Phase V2: 引用版本头替换硬编码

| 文件 | 行号 | 改前 | 改后 |
|------|------|------|------|
| `src/pipeline/pipeline.cpp` | 152 | `meta.sysal_version = "0.0.1";` | `meta.sysal_version = sysal::VERSION_STRING;` |
| `src/pipeline/pipeline.cpp` | 顶部 | 无 | `#include "sysal/version.hpp"` |
| `src/serialization/serialize.cpp` | ~1904 | `if(!ver_val->str_val.starts_with("0.0."))` | 用 `VERSION_MAJOR`/`VERSION_MINOR` 构建前缀比较 |

### Phase V3: 修复测试断言

| 文件 | 行号 | 改前 | 改后 |
|------|------|------|------|
| `tests/testbench.cpp` | ~297 | `assert(sys.meta.sysal_version == "0.0.1")` | `assert(sys.meta.sysal_version == sysal::VERSION_STRING)` |
| `tests/test_collect.cpp` | ~46 | `assert(sys.meta.sysal_version == "0.0.1")` | `assert(sys.meta.sysal_version == sysal::VERSION_STRING)` |
| `tests/test_serialization.cpp` | ~212/223 | JSON fixture 中硬编码 `"0.0.1"` | 保留 fixture 中的字符串字面量（fixture 是固定数据，不应引用代码常量）；断言改为 `== sysal::VERSION_STRING` |

test_serialization.cpp 的 JSON fixture（~line 158）中用 `"99.0.0"` 测试不兼容版本——这个保持不变。

### Phase V4: xmake.lua 添加版本声明

```lua
-- 在文件头部全局配置区
set_version("0.0.1")
```

xmake 的 `set_version` 会让 `xmake` 命令显示版本号，且可被 `xmake require` 等包管理功能使用。

### Phase V5: GitHub Release

1. `git tag v0.0.1`
2. `git push origin v0.0.1`
3. `gh release create v0.0.1 --title "v0.0.1" --notes "..."` 生成 release

Release notes 内容：
- sysal v0.0.1 首个版本
- 实现：公共 API（System::collect/refresh/Collect 位掩码）、全部数据模型、Linux 支持（procfs/sysfs）、JSON 序列化、raw replay 测试
- 不实现：性能评分、基准测试、跨平台、拓扑信息

## 不改动

- 设计文档中的 "0.0.1" 引用（历史文档，不改）
- 源码注释中的 "0.0.1" 引用（描述性，不改）
- `test_replay.cpp` 的 `!sys.meta.sysal_version.empty()` 断言（已经是好的写法）

## 执行顺序

```
V1 (创建 version.hpp) → V2 (引用替换) → V3 (测试修复) → V4 (xmake) → 验证构建 → commit → V5 (tag + release)
```

V1-V4 在一个 commit 中完成。

## 验证

1. `xmake -r` 构建成功
2. `xmake testbench` 输出中 version 显示 "0.0.1"
3. `git tag v0.0.1` + `gh release create` 成功
