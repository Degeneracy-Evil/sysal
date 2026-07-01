# sysal v0.0.2 代码质量评审报告

## 评审日期

2026-07-01

## 评审范围

sysal v0.0.2 全部代码：22 个公共头文件、34 个源文件、19 个测试文件。
评审由 5 个 Oracle agent 分 3 批并行执行，覆盖 9 个维度。

## 总分

| 维度 | v0.0.1 | v0.0.2 | 权重 | 加权分 | 变化 |
|------|--------|--------|------|--------|------|
| 设计文档忠实度 | 10 | 7 | 15% | 1.05 | ↓3 |
| API 优雅度 | 8 | 8 | 15% | 1.20 | — |
| 代码一致性 | 8 | 8 | 12% | 0.96 | — |
| Resolver 正确性 | 5 | 7 | 12% | 0.84 | ↑2 |
| 类型安全 | 9 | 8 | 8% | 0.64 | ↓1 |
| 错误处理健壮性 | 7 | 8 | 10% | 0.80 | ↑1 |
| Parser 健壮性 | 7 | 7 | 10% | 0.70 | — |
| 职责分离 | 9 | 8 | 10% | 0.80 | ↓1 |
| 测试质量 | 7 | 7 | 8% | 0.56 | — |
| **加权总分** | **8** | | **100%** | **8** | — |

加权总分计算：1.05 + 1.20 + 0.96 + 0.84 + 0.64 + 0.80 + 0.70 + 0.80 + 0.56 = 7.55 → **8**

---

## 各维度详评

### D1: 设计文档忠实度 7/10

**亮点**：
- 15 个数据模型结构（Platform/Cpu/Memory/Accelerator/Network/Storage/Pci/Software/Execution/System/RawStore/SnapshotMeta）与设计文档精确匹配
- 强类型系统（StrongId/NamedString/ScalarUnit）与 `rules/strong_typing.md` 完全一致
- 公共 API（System::collect/refresh、Collect 位掩码、序列化、replay）签名与文档一致

**扣分项**：
- [FAIL] `docs/design/rules/strong_typing.md:127` — StorageKind 枚举仍为 `{Nvme, Sata, Sas, Other}`，代码已改为 `{Nvme, Ssd, Hdd, Other}`
- [FAIL] `docs/design/rules/strong_typing.md:129` — IsaExtension 仍为 8 个值，代码已扩展到 17 个
- [FAIL] `docs/design/data_model/platform.md:38` — Virtualization 仍列 `container（bool）` 字段，代码已移除
- [WARN] `docs/design/testing/serialization.md:66` — 仍描述"手写 JSON 引擎"，v0.0.2 已替换为 nlohmann/json
- [WARN] `docs/design/architecture/pipeline.md:102` — 源文件布局仍列 `json.hpp`，该文件已删除
- [WARN] `docs/design/data_model/raw_store.md` — RawSource 枚举缺少 `ProcHostname`

### D2: API 优雅度 8/10

**亮点**：
- `System::collect(flags = full)` 静态工厂 + `refresh()` 简洁直觉
- `Collect` 位掩码 + `has()` + `basic`/`full` 预设，类型安全且可组合
- `SystemInfo` 扁平结构，直接成员访问，无冗余访问器
- 查询方法统一返回 `const T*` / `vector<const T*>`，非持有视图
- 序列化为非侵入式自由函数，不污染 System 类

**扣分项**：
- [WARN] `src/parser/pci.cpp:257-269` — lspci 合并创建部分填充的 PciDevice（仅 address + device_name），vendor/device_class 为空，用户无法区分"未知"与"空字符串"
- [WARN] `include/sysal/core/collect.hpp:45-47` — `full` 包含 `Collect::Raw`，混淆了"所有域"与"原始证据模式"
- [WARN] `src/parser/storage.cpp:127-134` — `StorageDevice::pci_address` 永远为 nullopt（B-2 TODO），dead API
- [WARN] `include/sysal/test/replay.hpp:33` — `collect_from_raw` 默认 `full`（含 Raw），replay 时 Raw 无意义

### D3: 代码一致性 8/10

**亮点**：
- Include 风格统一：全部 src 文件使用 `"sysal/..."` 引号
- Namespace 层次一致：`sysal::detail`（parser/resolver/pipeline）、`sysal::reader`、`sysal`
- Parser 签名统一：`std::optional<T> parse_X(const RawStore&, vector<string>&)`
- 表驱动分派一致：pipeline.cpp（ParserDispatch）和 sysfs.cpp（ReaderDispatch）风格平行

**扣分项**：
- [WARN] 5 个 parser .cpp 缺少 `/// @file` Doxygen 头：network.cpp、pci.cpp、storage.cpp、software.cpp、parse_utils.cpp
- [WARN] 测试断言风格不统一：17/19 用 `assert()`，test_serialization 和 test_replay 用 CHECK 宏
- [WARN] 注释分隔符风格不同：resource.cpp 用 `// ---- Cpu ----`，serialize.cpp 用 Unicode 线条
- [WARN] serialize.cpp include 顺序：`<nlohmann/json.hpp>` 在 `"sysal/..."` 之前

### D4: Resolver 正确性 7/10

**亮点**：
- [PASS] P0 修复 — 交叉检查不再是同义反复：`cross_check_cpu_visibility`（resolve.cpp:88-121）检测幻影 ID 和约束提示，与 `compute_cpu_visibility` 逻辑正交
- [PASS] P0 修复 — 可见性来源正确：Resolver 从 ExecutionContext 计算可见性，覆盖 parser 默认值
- [PASS] SystemInfo 组装完整：9 个字段全部 `value_or`（resolve.cpp:229-237）
- [PASS] 表驱动分派完整：9 个 Collect flag 全映射（pipeline.cpp:90-109）
- [PASS] 全部失败时抛出 SysalError（pipeline.cpp:137-142）

**扣分项**：
- [WARN] P0 部分修复 — 冲突解决仍是死代码：`resolve_conflict`（resolve.cpp:200-218）已实现但从未调用，`#pragma` 抑制未使用警告
- [WARN] pci.cpp lspci 合并（line 255）绕过 resolver 冲突框架，直接覆盖 device_name
- [WARN] init_backend/shutdown_backend 非异常安全（pipeline.cpp:118,159），无 RAII 守卫

### D5: 类型安全 8/10

**亮点**：
- 核心硬件模型（CPU/Memory/Storage/PCI/Network/Accelerator）强类型覆盖完整
- 序列化层正确拆包/重建强类型
- lspci 合并正确包装 DeviceName
- read_cpufreq P0 bug 已修复

**扣分项**：
- [FAIL] `include/sysal/model/platform.hpp:57` — `Firmware::bios_vendor` 仍为 `std::string` 而非 `Vendor`（v0.0.1 已指出，未修复）
- [WARN] Host::hostname/machine_id/product_name/serial、Architecture::name、Virtualization::hypervisor 均为 std::string
- [WARN] Software 模型 name/version/path 均为 std::string
- [WARN] pci.cpp:193 — sysfs "device" 文件 hex ID 暂存到 DeviceName，语义不精确

### D6: 错误处理健壮性 8/10

**亮点**：
- [PASS] 4 个静默 nullopt 解析器现在均发出警告（v0.0.1 问题已修复）
- [PASS] parse_uint/parse_hex 验证完整消费（v0.0.1 "123abc" 问题已修复）
- [PASS] read_cpufreq P0 bug 已修复
- [PASS] nlohmann/json 异常正确捕获并重抛为 SysalError
- [PASS] 版本兼容性检查已加入

**扣分项**：
- [WARN] cpu.cpp:423-424 仍使用 `.at()`，可抛出未捕获的 std::out_of_range
- [WARN] 多数解析器不检查 CollectStatus，Failed 记录与空文件不可区分
- [WARN] JSON 反序列化不校验枚举值范围
- [WARN] software.cpp 返回 nullopt 时仍无警告
- [WARN] lspci 不可解析行静默跳过

### D7: Parser 健壮性 7/10

**亮点**：
- [PASS] parse_uint/parse_hex 完整消费验证（v0.0.1 问题修复）
- [PASS] lspci 解析健壮：空行、缺字段、方括号名称、域名前缀均有处理
- [PASS] 内核版本解析有合理回退（无 # → release，无星期 → after_hash）
- [PASS] UEFI 检测基于目录存在性，不依赖内容
- [PASS] Storage rotational 缺失/异常值优雅降级为 Other

**扣分项**：
- [FAIL] cpu.cpp:300-309 — cpulist 范围展开无上限检查，`"0-18446744073709551615"` 导致 ~2^64 次循环（DoS 风险，v0.0.1 已指出）
- [FAIL] execution.cpp:38-41 — 同样的无界范围展开
- [WARN] accelerator.cpp 仍使用固定列位置访问 nvidia-smi CSV
- [WARN] execution.cpp:154 — euid 默认值为 0，无 Uid 行时 is_root 误报为 true
- [WARN] pci.cpp:206 — `static_cast<int64_t>(*val) == -1` 为死代码

### D8: 职责分离 8/10

**亮点**：
- Reader 只知道 RawStore，不知道域模型
- Parser 不调用 Reader，Resolver 不调用 Parser
- Namespace 边界清晰：`sysal::reader`、`sysal::detail`、`sysal`、`sysal::test`
- 所有 syscall/filesystem I/O 限制在 reader 层
- 序列化不导入 parser 内部

**扣分项**：
- [WARN] pipeline.cpp 仍含两个 namespace 块（detail + test），v0.0.1 已指出
- [WARN] pci.cpp lspci 合并在 parser 层做多源冲突解决，绕过 resolver 的 TrustLevel 框架

### D9: 测试质量 7/10

**亮点**：
- [PASS] test_resolve test 7 误导命名已修复（改为"幻影 ID 检测"）
- [PASS] 新增测试质量高：ISA 17 扩展全验证、lspci 合并 4 场景、storage rotational 3 边界
- [PASS] parse_utils 边界测试强：拒绝部分消费、拒绝非法十六进制
- [PASS] test_resolve 9 个测试覆盖可见性/幻影/约束/默认值

**扣分项**：
- [FAIL] 16/19 测试文件用 `assert()`，`-DNDEBUG` 下静默失效
- [WARN] testbench 815 行 ~95% 为打印输出，仅 ~15 个断言
- [WARN] 序列化 round-trip 仅验证 6/~50 个字段
- [WARN] test_collect 仍仅 4 个 smoke test
- [WARN] test_reader 环境依赖（读 live /proc /sys），不可复现
- [WARN] 无网络/存储可见性测试、无 parser 错误路径测试

---

## 关键问题优先级排序

| 优先级 | 问题 | 位置 | 影响 |
|--------|------|------|------|
| P0 | cpulist/cgroup 范围展开无上限检查 | cpu.cpp:300, execution.cpp:38 | DoS 风险 |
| P0 | 冲突解决仍是死代码 | resolve.cpp:200-218 | 设计文档要求未实现 |
| P1 | 设计文档 5 处与代码不一致 | strong_typing.md, platform.md, serialization.md, pipeline.md, raw_store.md | 文档误导 |
| P1 | Firmware::bios_vendor 仍为 std::string | platform.hpp:57 | 类型安全 |
| P1 | assert() 可被 NDEBUG 禁用 | 16/19 测试文件 | 测试可靠性 |
| P1 | execution.cpp is_root 默认 0 误报 | execution.cpp:154 | 正确性 |
| P2 | cpu.cpp .at() 未捕获异常 | cpu.cpp:423 | 健壮性 |
| P2 | 多数 parser 不检查 CollectStatus | 除 pci.cpp 外全部 | 健壮性 |
| P2 | JSON 反序列化不校验枚举范围 | serialize.cpp 多处 | 健壮性 |
| P2 | lspci 合并创建部分填充 PciDevice | pci.cpp:257-269 | API 一致性 |
| P2 | 5 个 parser .cpp 缺 @file 头 | network/pci/storage/software/parse_utils | 代码一致性 |
| P2 | testbench 仍为 demo 非 test | testbench.cpp | 测试质量 |
| P2 | 序列化 round-trip 覆盖率低 | test_serialization.cpp | 测试质量 |
| P2 | software.cpp 静默返回 nullopt | software.cpp:105 | 错误处理 |

---

## v0.0.1 → v0.0.2 改善对比

### 已修复的 v0.0.1 问题
| 问题 | v0.0.1 | v0.0.2 |
|------|--------|--------|
| 交叉检查同义反复 | P0 | ✅ 修复 |
| 可见性来源违反 | P0 | ✅ 修复 |
| read_cpufreq 忽略 package_id | P0 | ✅ 修复 |
| parse_uint 不验证完整消费 | P1 | ✅ 修复 |
| 4 个 parser 静默返回 nullopt | P1 | ✅ 修复 |
| test_resolve test 7 误导命名 | P2 | ✅ 修复 |

### 新引入的问题
| 问题 | 来源 | 严重性 |
|------|------|--------|
| 设计文档 5 处过时 | R5 变更未同步文档 | P1 |
| lspci 合并部分填充 PciDevice | R5c | P2 |
| pci.cpp lspci 合并绕过冲突框架 | R5c | P2 |

### 仍存在的问题
| 问题 | v0.0.1 优先级 | 状态 |
|------|--------|------|
| 冲突解决未实现 | P0 | 部分修复（helper 已实现但未调用） |
| 范围展开无上限 | P1 | 未修复 |
| Firmware::bios_vendor 类型 | P2 | 未修复 |
| assert() 可被 NDEBUG 禁用 | P2 | 未修复 |
| 5 个 parser 缺 @file | P2 | 未修复 |
| testbench 非 test | P2 | 改善（加了 ~15 断言）但仍主要是 demo |
