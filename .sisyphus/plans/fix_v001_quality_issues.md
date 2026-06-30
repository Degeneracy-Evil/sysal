# 质量评审问题修复计划

## 来源

基于 `docs/quality_reports/v001_initial_rewrite.md` 中的 11 个问题（3 个 P0 + 3 个 P1 + 5 个 P2）。

## 约束

- 每个阶段必须 `utils/check.sh` 全绿
- 每个阶段 = 1 个原子 commit
- 修改代码的同时更新对应的测试
- 不动设计文档（已冻结）
- 更新 `docs/devlog.md`

---

## Phase F1 — P0 修复：read_cpufreq + 交叉检查 + 冲突解决 (L)

### F1a: 修复 read_cpufreq 忽略 package_id

**问题**：`read_cpufreq` 标记 `package_id` 为 `[[maybe_unused]]`，遍历全部 SysfsCpu 记录取第一个 `base_frequency`，导致多 socket 系统所有 package 获得 cpu0 的频率。

**修复**：`src/parser/cpu.cpp`
- `read_cpufreq` 改为接收该 package 包含的逻辑 CPU ID 列表
- 遍历 SysfsCpu 记录时，通过 path 中的 `cpuN` 提取 CPU 编号，只处理属于该 package 的 CPU 记录
- 调用点（约 line 430-445）传入每个 package 的 CPU 列表

**测试**：`tests/test_parse_cpu.cpp`
- 新增测试：2 个 package，各自有不同的 cpufreq 值，断言各 package 的 base_frequency/max_frequency 不同

### F1b: 修复交叉检查同义反复

**问题**：`compute_cpu_visibility` 从 `visible_logical_cpu_ids` 设置 `visible_to_current_process`，然后 `cross_check_cpu_visibility` 对比同一组数据，永远不可能不一致。

**设计意图分析**：
- `execution.md`：`visible_to_current_process` 是事实来源，索引是派生的便利字段
- `conflict_resolution.md`：可见性冲突时执行上下文胜出
- 结论：Resolver 应该用执行上下文计算可见性（这是"计算"），交叉检查应该检测有意义的数据不一致

**修复**：`src/resolver/resolve.cpp`
- 保留 `compute_*_visibility`（从执行上下文计算可见性，设置 `visible_to_current_process`）
- 重写 `cross_check_*_visibility`，改为检测以下有意义的不一致：
  1. **幻影 ID**：`visible_logical_cpu_ids` 包含 Cpu 模型中不存在的 CPU ID → `[visibility_mismatch] cpu_N: in_index=true but cpu does not exist in model`
  2. **约束提示**：当 cpuset 限制了可见性（索引非空且小于模型中 CPU 总数）→ `[constraint] cpu visibility restricted: N total, M visible`（信息性警告，不是错误）
  3. 对加速器同理：检测幻影 ID 和约束提示

**测试**：`tests/test_resolve.cpp`
- 测试 7 重写：构造 `visible_logical_cpu_ids` 包含一个 Cpu 模型中不存在的 CPU ID，断言生成 `[visibility_mismatch]` 警告
- 新增测试：构造 cpuset 限制为 2/4 CPU，断言生成 `[constraint]` 警告，且 `visible_to_current_process` 正确设置
- 新增测试：加速器幻影 ID 测试

### F1c: 实现冲突解决框架

**问题**：`resolve.cpp:181-186` 仅有注释占位，设计文档要求的 5 类规则和 `[conflict]` 格式零实现。

**修复**：`src/resolver/resolve.cpp`
- 新增匿名 namespace helper：`resolve_conflict(field, src1, val1, src2, val2, trust_order) -> (adopted_val, optional<warning>)`
  - 按 `trust_order` 比较两个来源的信任等级
  - 高信任来源胜出
  - 若值不同，生成 `[conflict] <field>: <src1>=<val>, <src2>=<val>, adopted=<winner>` 格式警告
- 在 `resolve()` 中，对 v0.0.1 中可能有多来源的字段调用此 helper：
  - GPU 名称：nvidia-smi vs lspci（若两者都有数据）
  - GPU 显存：nvidia-smi vs sysfs（若两者都有数据）
  - NUMA 归属：sysfs PCI vs sysfs NUMA cpulist（若两者都提供了映射）
- 对于 v0.0.1 中只有单一来源的字段，不调用（无冲突可能）
- 这不是空框架——是真正的冲突检测逻辑，只是 v0.0.1 中触发场景少

**测试**：`tests/test_resolve.cpp`
- 新增测试：构造两个来源对同一字段提供不同值，断言高信任来源胜出 + `[conflict]` 格式警告生成
- 新增测试：两个来源提供相同值，断言无冲突警告

**成功标准**：`utils/check.sh` 全绿；新增测试覆盖幻影 ID、约束提示、冲突解决

**Commit**：`fix(resolver): non-tautological cross-check, conflict resolution framework, cpufreq per-package`

---

## Phase F2 — P1 修复：parse_uint + collect() + parser warnings (M)

### F2a: 修复 parse_uint/parse_hex 不验证完整消费

**问题**：`parse_uint("123abc")` 静默返回 123，因为只检查 `ec` 不检查 `ptr == end`。

**修复**：`src/parser/parse_utils.cpp`
- `parse_uint`：在 `ec` 检查后增加 `if(ptr != trimmed.data() + trimmed.size()) return std::nullopt;`
- `parse_hex`：同上

**测试**：`tests/test_parse_utils.cpp`
- 新增：`parse_uint("123abc")` 返回 nullopt
- 新增：`parse_hex("ffxyz")` 返回 nullopt
- 确保现有合法输入仍正常

### F2b: collect() 在全部 parser 失败时抛 SysalError

**问题**：`collect()` 永不抛 `SysalError`，即使全部 parser 返回 nullopt，违反文档约定。

**修复**：`src/pipeline/pipeline.cpp`
- 在 `run_replay` 中，`record_collector_status` 之后，检查：如果所有请求的域都失败了（`succeeded_collectors` 为空且 `failed_collectors` 非空），抛 `SysalError(ErrorKind::CollectionFailed, "all requested collectors failed")`
- 不影响部分失败的情况（部分失败仍返回带默认值的 System + warnings）

**测试**：`tests/test_collect.cpp`
- 新增：用空 RawStore 调用 `collect_from_raw(raw, Collect::Cpu)`，断言抛出 `SysalError`
- 保留现有 smoke test

### F2c: 4 个 parser 空数据时添加 warning

**问题**：network/pci/storage/accelerator 空数据时静默返回 nullopt，与 cpu/memory/platform 不一致。

**修复**：
- `src/parser/network.cpp`：返回 nullopt 前添加 `warnings.push_back("parse_network: 缺少 SysfsNet 数据")`
- `src/parser/pci.cpp`：同模式
- `src/parser/storage.cpp`：同模式
- `src/parser/accelerator.cpp`：同模式

**测试**：现有测试已覆盖空数据返回 nullopt 的场景，增加断言 `!warnings.empty()`

**成功标准**：`utils/check.sh` 全绿

**Commit**：`fix(parser): parse_uint full consumption, collect() throws on total failure, empty-data warnings`

---

## Phase F3 — P2 修复：一致性 + 测试质量 (M)

### F3a: 统一 parser .cpp 的 /// @file 头

**修复**：给以下 5 个文件添加 `/// @file` + `/// @brief` + `/// @details` Doxygen 头：
- `src/parser/cpu.cpp`
- `src/parser/execution.cpp`
- `src/parser/platform.cpp`
- `src/parser/accelerator.cpp`
- `src/parser/memory.cpp`

### F3b: 统一 include 风格

**修复**：全项目统一用引号 `"sysal/..."` 引用项目头文件（不用尖括号）：
- `src/serialization/serialize.cpp`：`<sysal/...>` → `"sysal/..."`
- `tests/test_replay.cpp`：同上
- 检查其他文件是否也有混用

### F3c: testbench 添加基本断言

**修复**：`tests/testbench.cpp`
- 在打印输出后添加基本断言：`assert(!sys.info.cpu.logical_cpus.empty())`、`assert(sys.meta.succeeded_collectors.size() > 0)` 等
- 保留打印输出（它仍是 demo），但加上断言使其也是有效测试

### F3d: 修复 test_resolve 测试 7

**修复**：`tests/test_resolve.cpp`
- 已在 F1b 中重写——确保测试名称与实际测试内容一致

### F3e: 防止 assert() 被 NDEBUG 禁用

**修复**：在 `tests/` 目录下新增 `test_assert.hpp`：
```cpp
#pragma once
#include <cassert>
#ifndef NDEBUG
// debug mode: assert works normally
#else
#error "Tests must not be compiled with -DNDEBUG"
#endif
```
- 在所有测试文件的第一行 `#include "test_assert.hpp"`
- 或者更简单：在 xmake.lua 的 test_target helper 中添加 `add_cxxflags("-Werror=macro-redefined", {force = true})` 并 `#define NDEBUG 0` —— 但这太 hacky
- **最终方案**：在 xmake.lua 的 test_target helper 中确保不传 `-DNDEBUG`（xmake release 模式可能默认加），并在每个测试文件顶部加 `#include <cassert>` 后 `#undef NDEBUG`

**成功标准**：`utils/check.sh` 全绿

**Commit**：`fix: consistency (/// @file, include style, testbench assertions, NDEBUG guard)`

---

## 依赖与执行顺序

```
F1a (cpufreq) ──┐
F1b (cross-check)├── 并行，无依赖 ──→ F1 commit
F1c (conflict) ──┘

F2a (parse_uint) ──┐
F2b (collect)    ──┼── 并行，无依赖 ──→ F2 commit
F2c (warnings)   ──┘

F3a-f (一致性) ──→ F3 commit
```

F1、F2、F3 之间无硬依赖，但建议顺序执行（F1 最重要）。F1 内部 3 个子任务可并行。F2 内部 3 个子任务可并行。

## 风险评估

- **F1b 最高风险**：交叉检查的重写涉及对设计意图的理解。关键决策：保留"执行上下文计算可见性"的行为（符合 conflict_resolution.md），但交叉检查检测幻影 ID 和约束提示（有意义的不一致检测）
- **F1c 中风险**：冲突解决框架需要正确实现信任优先级比较，但 v0.0.1 触发场景少，风险可控
- **F2a 低风险**：加一行 `ptr == end` 检查，但可能暴露之前被静默接受的畸形输入测试
- **F3e 低风险但繁琐**：NDEBUG 防护需要触及所有测试文件

## 验证

每个 phase 完成后：
1. `utils/check.sh` 全绿
2. 新增/修改的测试通过
3. 重新跑对应的评审维度（可选），确认分数提升
