# sysal v0.0.1 代码质量评审报告

## 评审日期

2026-07-01

## 评审范围

sysal v0.0.1 全部重写代码：22 个公共头文件、28 个源文件、20 个测试文件。
评审由 5 个 Oracle agent 分 3 批并行执行，覆盖 9 个维度。

## 总分

| 维度 | 分数 | 权重 | 加权分 |
|------|------|------|--------|
| 设计文档忠实度 | 10 | 15% | 1.50 |
| API 优雅度 | 8 | 15% | 1.20 |
| 代码一致性 | 8 | 12% | 0.96 |
| Resolver 正确性 | 5 | 12% | 0.60 |
| 类型安全 | 9 | 8% | 0.72 |
| 错误处理健壮性 | 7 | 10% | 0.70 |
| Parser 健壮性 | 7 | 10% | 0.70 |
| 职责分离 | 9 | 10% | 0.90 |
| 测试质量 | 7 | 8% | 0.56 |
| **加权总分** | | **100%** | **8** |

---

## 各维度详评

### 设计文档忠实度 10/10

23 个设计文档中的每一个 struct、字段、枚举值、API 签名都在代码中精确匹配。
无遗漏字段、无多余字段、无类型重命名错误。`IsaExtension` 恰好 8 个值，
`RawSource` 恰好 23 个值，`Collect` 位掩码与预设完全一致。

### API 优雅度 8/10

**亮点**：
- `System::collect(flags = full)` 简洁直觉（system.hpp:54）
- `SystemInfo` 扁平结构无冗余嵌套（system.hpp:32-43）
- `Collect` 位掩码比 builder 模式更轻量（collect.hpp:30-47）
- 强类型系统防止编译期混用（strong_id.hpp, units.hpp, value_types.hpp）
- 查询方法统一返回非持有指针（cpu.hpp:72-101, accelerator.hpp:43-64）

**扣分项**：
- `StrongId` 用 `value()` 访问器，`ScalarUnit`/`NamedString` 直接暴露 `value` 成员——三个类似包装器两种封装风格（strong_id.hpp:31 vs units.hpp:18, value_types.hpp:35）
- `has(Collect, Collect)` 是自由函数，缺少 `operator&`（collect.hpp:36）
- `Cpuset` 字段是原始内核格式字符串而非解析后的 ID 列表（execution.hpp:49-53）
- `Architecture::byte_order` 是字符串而非枚举（platform.hpp:51）

### 代码一致性 8/10

**亮点**：
- 命名规则全项目统一
- parser 签名统一：`parse_xxx(const RawStore&, vector<string>&) -> optional<T>`
- `[[nodiscard]]`/`[[maybe_unused]]` 使用一致
- 匿名 namespace 用于文件局部 helper

**扣分项**：
- 5 个 parser .cpp 缺少 `/// @file` Doxygen 头，其他所有 .cpp 都有（cpu.cpp:1, execution.cpp:1, platform.cpp:1, accelerator.cpp:1, memory.cpp:1）
- include 风格不统一：serialize.cpp 用 `<sysal/...>` 尖括号，pipeline.cpp 用 `"sysal/..."` 引号
- 测试断言风格不统一：有的用 `assert()`，有的用自定义 `CHECK()` 宏
- `raw_store.cpp` 用 `std::ranges`，`resource.cpp` 用手写 for 循环

### Resolver 正确性 5/10

**正确部分**：
- 可见性计算 happy path 正确：空约束→全可见，ID 比较正确（resolve.cpp:23-30）
- `SystemInfo` 组装用 `std::move` + `value_or` 安全完整，9 个字段全覆盖（resolve.cpp:159-167）

**严重问题**：
1. **交叉检查是同义反复**（resolve.cpp:171-179）——`compute_cpu_visibility` 从 `visible_logical_cpu_ids` 设置 `visible_to_current_process`，然后 `cross_check_cpu_visibility` 对比同一组数据，永远不可能发现不一致
2. **冲突解决完全是注释占位**（resolve.cpp:181-186）——设计文档定义了 5 类规则和 `[conflict]` 格式，代码中零实现
3. **违反文档定义的真值来源**（resolve.cpp:159-171 vs execution.md:61）——文档说 `visible_to_current_process` 是真值来源，但 Resolver 从索引覆写它

### 类型安全 9/10

所有已定义的强类型（8 个 StrongId、6 个 NamedString、3 个 ScalarUnit、PciAddress）在模型结构和 parser 构造中全部正确使用。`StrongId` 构造函数 `explicit` 防止隐式转换。Parser 内部中间表示用原始类型，输出时正确包装为强类型。

**扣分项**：`Firmware::bios_vendor` 用 `std::string` 而非 `Vendor`（platform.hpp:57），与 `Host::vendor` 用 `Vendor` 语义不一致。

### 错误处理健壮性 7/10

**亮点**：
- parser 统一返回 `nullopt` + warning
- reader 用 `error_code` 重载避免异常（sysfs.cpp:51,101,141）
- 序列化精确抛出 `SysalError`，版本检查实现（serialize.cpp:1887-1908）

**扣分项**：
- 4 个 parser 空数据时静默返回 `nullopt` 无 warning（network.cpp:70-74, pci.cpp:63-67, storage.cpp:67-72, accelerator.cpp:160-165）
- `cpu.cpp` 用 `.at()` 可能抛 `out_of_range`（cpu.cpp:380-381）
- `read_cpufreq` 忽略 `package_id`，多 socket 系统所有 package 获得 cpu0 的频率——**逻辑 bug**（cpu.cpp:173-206）
- `collect()` 永不抛 `SysalError`，即使全部 parser 失败（system.cpp:16-20）
- parser 不检查 `CollectStatus`，Failed 记录被当作 Success 处理

### Parser 健壮性 7/10

**亮点**：
- 空输入、缺字段、数值解析失败均有 fallback + warning
- PCI 地址十六进制解析正确（parse_utils.cpp:88-146）
- NUMA -1 处理正确（pci.cpp:113-140）
- 内存单位转换覆盖 MiB/GiB/KiB/B（accelerator.cpp:56-99）

**扣分项**：
- `parse_uint`/`parse_hex` 不验证 `ptr == end`，`"123abc"` 静默解析为 `123`（parse_utils.cpp:64-68, 80）——影响所有 parser
- `read_cpufreq` 忽略 `package_id`——多 socket 频率错误（cpu.cpp:173-206）
- 范围展开 `0-18446744073709551615` 无上限检查——DoS 风险（cpu.cpp:261, execution.cpp:34）
- nvidia-smi CSV 假设固定列顺序，无 header-based 列映射（accelerator.cpp:17-39）
- `accelerator.cpp` 的 NUMA -1 处理与 `pci.cpp` 不一致

### 职责分离 9/10

include 依赖形成干净的有向图：Reader → RawStore ← Parser → ParseResult → Resolver → SystemInfo。
零 syscall 泄漏到 reader 层之外。命名空间边界严格。Parser 只读 RawStore，Resolver 只读 ParseResult。

**扣分项**：`pipeline.cpp` 跨 `detail` 和 `test` 两个 namespace（pipeline.cpp:24,182-189）。

### 测试质量 7/10

**亮点**：
- 12 个 parser 测试覆盖 happy path + 边界 + 错误路径，有精确值断言
- `test_json.cpp` 卓越——9 个畸形输入错误路径 + 真正的 round-trip 验证
- `test_raw_store_io.cpp` 覆盖 FileNotFound/DeserializationError 错误路径

**扣分项**：
- `testbench.cpp` 311 行零断言——是 demo 不是 test
- `test_resolve.cpp` 测试 7 名为"交叉校验——不一致时产生警告"但断言 `!has_mismatch`——误导性命名，冲突路径完全未测试（test_resolve.cpp:183-226）
- `test_collect.cpp` 仅 54 行 3 个 smoke test，只检查非空
- 序列化 round-trip 只验证 5 个字段（9 个子系统中）
- 16/20 个测试用 `assert()`，`-DNDEBUG` 下全部静默通过

---

## 关键问题优先级排序

| 优先级 | 问题 | 位置 | 影响 |
|--------|------|------|------|
| P0 | `read_cpufreq` 忽略 package_id | cpu.cpp:173 | 多 socket 频率错误 |
| P0 | 交叉检查是同义反复 | resolve.cpp:171-179 | 交叉检查永不触发 |
| P0 | 冲突解决零实现 | resolve.cpp:181-186 | 设计文档要求完全未实现 |
| P1 | `parse_uint` 不验证完整消费 | parse_utils.cpp:64 | 所有 parser 静默截断 |
| P1 | `collect()` 永不抛 SysalError | system.cpp:16 | 违反文档约定 |
| P1 | 4 个 parser 空数据无 warning | network/pci/storage/accelerator | 不一致 |
| P2 | parser .cpp 缺 `/// @file` | 5 个 parser 文件 | 代码一致性 |
| P2 | include 风格不统一 | serialize.cpp vs pipeline.cpp | 代码一致性 |
| P2 | testbench 不是测试 | testbench.cpp | 测试质量 |
| P2 | test_resolve 测试 7 误导 | test_resolve.cpp:183 | 测试质量 |
| P2 | assert() 可被 NDEBUG 禁用 | 16/20 测试文件 | 测试质量 |
