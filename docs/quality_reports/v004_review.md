# sysal v0.0.4 代码质量评审报告

## 评审日期

2026-07-05

## 评审范围

sysal v0.0.4 全部代码。v0.0.3 tag 后 10 个 commit，涵盖 4 项新功能（Network IP/PCI、Storage df-Th、Memory DIMM）+ syscall 优化 + 文档对齐。
评审由 5 个 Oracle agent 分 3 批并行执行，覆盖 9 个维度。

## 总分

| 维度 | v0.0.2 | v0.0.4 | 权重 | 加权分 | 变化 |
|------|--------|--------|------|--------|------|
| D1 设计文档忠实度 | 7 | 9 | 15% | 1.35 | ↑2 |
| D2 API 优雅度 | 8 | 8 | 15% | 1.20 | — |
| D3 代码一致性 | 8 | 8 | 12% | 0.96 | — |
| D4 Resolver 正确性 | 7 | 7 | 12% | 0.84 | — |
| D5 类型安全 | 8 | 7 | 8% | 0.56 | ↓1 |
| D6 错误处理健壮性 | 8 | 7 | 10% | 0.70 | ↓1 |
| D7 Parser 健壮性 | 7 | 8 | 10% | 0.80 | ↑1 |
| D8 职责分离 | 8 | 9 | 10% | 0.90 | ↑1 |
| D9 测试质量 | 7 | 8 | 8% | 0.64 | ↑1 |
| **加权总分** | **8** | | **100%** | **8** | — |

加权总分计算：1.35 + 1.20 + 0.96 + 0.84 + 0.56 + 0.70 + 0.80 + 0.90 + 0.64 = 7.95 → **8**

---

## 各维度详评

### D1: 设计文档忠实度 9/10（↑2）

**亮点**：
- v0.0.4 专门的文档对齐 commit 修复了所有已知的文档-代码偏差
- 4 个新 RawSource（IfAddrs, DfTh, Udevadm, SysfsEdac）均已入文档
- DimmInfo 13 字段、StorageDevice 新字段（mount_point, fs_type）、NetworkInterface 新字段（addresses, pci_address）均与文档精确匹配
- 新增 data_source_guideline.md 文档完整记录了 syscall > file > command 原则

**扣分项**：
- [WARN] `raw_store.md` 中 RawSource 枚举分组与代码不一致：文档按功能分组，代码按添加顺序追加在末尾
- [WARN] `public_api.md:187-189` 示例 `full | Collect::Raw` 与代码矛盾：代码中 `full` 已包含 `Collect::Raw`，示例暗示不包含
- [WARN] `memory.hpp:3` 文件头注释仍为 "NumaMemory、Memory"，未更新加入 DimmInfo

### D2: API 优雅度 8/10（—）

**亮点**：
- System 类简洁：`collect(flags=full)` + `refresh()`，公开成员无 accessor
- Collect 位掩码 + `operator|` + `has()` + `basic`/`full` 预设，可组合且类型安全
- 新字段命名一致：addresses、pci_address、mount_point、fs_type 均遵循既有模式
- DimmInfo 的 optionality 设计精准（speed_mts/manufacturer 等可选，locator/type 必填）
- 查询方法模式统一：find_* / visible() / by_kind()，返回 `const T*` / `vector<const T*>`

**扣分项**：
- [WARN] `serialize.cpp:118,121` — RawSource 和 CollectStatus 反序列化未用 `validate_enum()` 边界检查，其他枚举类型（Arch, StorageKind 等）都有
- [WARN] `software.hpp:53` — Library::kind 为 `std::string`，在强类型 API 中属于 stringly-typed
- [WARN] Network::find(InterfaceName) vs Cpu::find_*(StrongId) 参数类型不一致（领域语义合理但表面不一致）

### D3: 代码一致性 8/10（—）

**亮点**：
- 全部 6 个 parser 文件结构完全一致：文件头 → include → 匿名命名空间 → helper → 公开 parse_X 函数
- include 顺序统一：own header → project headers → stdlib
- 警告消息格式统一：`"parse_X: <description>"`
- 新 parser（network IP、storage df-Th、memory DIMM）与既有 parser 风格一致

**扣分项**：
- [WARN] 3 个 parser 文件中存在字节级重复的 `extract_filename_from_path` 函数（network.cpp:44, pci.cpp:55, storage.cpp:72），且 storage.cpp 命名不同（`extract_filename` vs `extract_filename_from_path`）
- [WARN] 未使用参数处理方式不一：多数用 `[[maybe_unused]]`，cpu.cpp:250-251 用 `(void)` cast
- [WARN] `has_success()` 便利方法只在 execution.cpp:345 用了一次，其他 6 个 parser 都手动循环查找 Success 记录

### D4: Resolver 正确性 7/10（—）

**亮点**：
- 可见性计算正确：CPU（cpuset→visible）、Accelerator（CUDA_VISIBLE_DEVICES→visible）、Network（全可见，已文档化限制）
- 交叉校验有效：幻影 ID 检测 + 约束提示
- `value_or(X{})` 模式正确处理空/缺失域
- 跨域依赖在 procfs.cpp:179-183 有文档化和正确的条件补充采集

**扣分项**：
- [WARN] `pipeline.cpp:57-81` — `record_collector_status` 检查全部 9 个域，不区分是否请求。未请求的域被计入 `failed_collectors`，导致 `SnapshotMeta.failed_collectors` 误导
- [WARN] `pipeline.cpp:118-159` — backend init/shutdown 无异常安全：如果 resolve() 抛异常，shutdown_backend() 不会执行（当前为 no-op，但 NVML 后端接入后会成为资源泄漏）
- [WARN] `resolve.cpp:114` — 冗余空检查（line 91 已 return，line 114 再次检查 `!empty()`）

### D5: 类型安全 7/10（↓1）

**亮点**：
- 所有 ID 一致使用 StrongId
- 所有内存大小一致使用 MemorySize
- 新 Network 字段类型正确：addresses 为 `vector<IpAddress>`，pci_address 为 `optional<PciAddress>`
- 序列化层保留类型安全：提取 `.value()` 输出，反序列化时用显式构造重建强类型

**扣分项**：
- [WARN] `memory.hpp:32-33` — `speed_mts` 和 `configured_speed_mts` 为 `optional<uint32_t>`，但 `Frequency` ScalarUnit 已存在且语义相关
- [WARN] `memory.hpp:34` — `manufacturer` 为 `optional<string>`，但 `Vendor`（NamedString）已存在且在其他模型中使用
- [WARN] `storage.hpp:28-29` — `mount_point` 和 `fs_type` 为 `optional<string>`，未创建 NamedString 标签
- [WARN] 预存问题：Software 全部字段为 raw string，Platform Os/Kernel 字段为 raw string，Process.pid/uid 为 raw integer

### D6: 错误处理健壮性 7/10（↓1）

**亮点**：
- Pipeline 层异常 vs 警告边界正确：全部失败→SysalError，部分失败→warnings
- Reader 层失败处理完善：read_proc_file/read_cmd/read_uname/read_hostname/read_ifaddrs 都在失败时记录 Failed 状态
- Parser 层缺失数据正确处理：返回 nullopt + warning
- SysalError 设计简洁：ErrorKind + message + noexcept what()

**扣分项**：
- [WARN] `memory.cpp:77-78` — `parse_numa_meminfo` 标记 warnings 为 `[[maybe_unused]]`，静默吞掉 NUMA 内存值解析失败
- [WARN] `memory.cpp:124-263` — `parse_udevadm_dimms` 对每条畸形行静默 `continue`，无任何 warning
- [WARN] `network.cpp:115-127` — IfAddrs 解析静默跳过畸形行（无空格分隔、空 ifname/ip）
- [WARN] `network.cpp:172-176` — PCI 地址解析失败静默保持 nullopt，无 warning
- [WARN] `storage.cpp:144-147` — df -Th 行字段数 < 7 静默跳过
- [WARN] `storage.cpp:218` — 分区匹配只取第一个匹配，多分区设备只记录第一个分区的挂载点，静默丢数据
- [WARN] `platform.cpp:287-295` — hostname 缺失时静默留空，不像 os-release/proc-version 那样发 warning

### D7: Parser 健壮性 8/10（↑1）

**亮点**：
- 所有 parser 一致检查 `rec->status != CollectStatus::Success` 并跳过失败记录
- 空输入、缺失字段、Failed 状态均优雅处理
- 无崩溃路径：无空指针解引用、无越界访问、无 UB（`<cctype>` 函数正确使用 `static_cast<unsigned char>()`）
- Memory DIMM 双源 fallback（udevadm → EDAC）设计良好
- IPv6 透明处理：parser 不区分 IPv4/IPv6，reader 正确用 `inet_ntop` 处理 AF_INET6
- df -Th 挂载点含空格正确处理（join fields[6+]）

**扣分项**：
- [WARN] `pci.cpp:215-223` — 死代码：`parse_uint`（无符号 from_chars）无法解析 "-1"，该分支不可达
- [WARN] `storage.cpp:211` — 分区前缀匹配 `df_name.starts_with(dev_name)` 可能误匹配（"sda" 匹配 "sdab"）
- [WARN] `storage.cpp:127-167` — df -Th 多行格式（长设备名换行）未处理

### D8: 职责分离 9/10（↑1）

**亮点**：
- Pipeline 只编排：调用 reader → dispatch parser → 调用 resolver，无解析/解决逻辑
- Resolver 只组装 + 交叉校验：移动 ParseResult 字段，计算可见性，无原始数据解析
- Parser 纯函数：只消费 RawStore，无 `ifstream`/`popen`/`getifaddrs`/`uname`/`gethostname`/`getenv`/`read_file`/`read_command`/`read_symlink`/`fs::` 调用
- Reader 层正确放置所有 syscall：uname()、gethostname()、getifaddrs()、readlink() 均在 procfs.cpp/sysfs.cpp
- 序列化层只触碰公共模型类型，无 parser/resolver 内部头文件依赖
- data_source_guideline.md 文档化了 syscall > file > command 原则

**扣分项**：
- [WARN] `procfs.cpp:179-267` — 跨域依赖处理（Cpu→Platform, Pci→Network, Software→Accelerator）在 reader 层增加了条件补充采集复杂度，但不违反职责分离

### D9: 测试质量 8/10（↑1）

**亮点**：
- 新功能场景覆盖良好：Network IP/PCI（4 个测试）、Storage df-Th（2 个测试）、Memory DIMM（3 个测试）、Platform uname 新格式（已有测试更新）
- CHECK 宏一致使用，无 raw assert
- 测试独立：每个测试用例使用局部 RawStore，无共享状态
- 空输入→nullopt + warnings 对每个 parser 都有测试
- 测试描述清晰（`// ---- 测试 N: ... ----`）

**扣分项**：
- [FAIL] `test_parse_platform.cpp` — 无 ProcHostname/gethostname 解析测试，新的 gethostname syscall 路径在 parser 层未测试
- [FAIL] `test_serialization.cpp:34-118` — 序列化 round-trip 测试不验证任何 v0.0.4 新字段（DimmInfo、mount_point、fs_type、addresses、pci_address）
- [FAIL] `test_reader.cpp` — 5 个新 reader 函数（read_hostname, read_ifaddrs, read_udevadm, DfTh, read_edac_sysfs）无单元测试覆盖
- [WARN] `test_replay.cpp:61,92` — 占位 `CHECK(true)` 断言无验证价值
- [WARN] 新功能无畸形输入测试（garbled df header、truncated udevadm、invalid IPv6）
- [WARN] `test_serialization.cpp:38-40` — round-trip 依赖 live `System::collect()`，环境依赖、非确定性

---

## 关键问题优先级排序

### P0（必须修复）

无。当前无阻塞发布的问题。

### P1（应该修复）

| # | 维度 | 问题 | 文件 | 描述 |
|---|------|------|------|------|
| 1 | D5 | DimmInfo.speed_mts 类型 | `memory.hpp:32-33` | `optional<uint32_t>` 应使用 Frequency 或新建 TransferRate ScalarUnit |
| 2 | D5 | DimmInfo.manufacturer 类型 | `memory.hpp:34` | `optional<string>` 应使用 `optional<Vendor>` |
| 3 | D5 | StorageDevice.mount_point/fs_type 类型 | `storage.hpp:28-29` | `optional<string>` 应创建 NamedString 标签 |
| 4 | D4 | record_collector_status bug | `pipeline.cpp:57-81` | 未请求的域被计入 failed_collectors |
| 5 | D6 | NUMA 内存解析静默失败 | `memory.cpp:77-78` | `[[maybe_unused]]` 抑制了 warning，NUMA 值解析失败无诊断 |
| 6 | D9 | 序列化 round-trip 不验证新字段 | `test_serialization.cpp` | DimmInfo/mount_point/fs_type/addresses/pci_address 未 round-trip 测试 |
| 7 | D9 | 新 reader 函数无单元测试 | `test_reader.cpp` | read_hostname/read_ifaddrs/read_udevadm/DfTh/read_edac_sysfs 无测试 |

### P2（建议修复）

| # | 维度 | 问题 | 文件 | 描述 |
|---|------|------|------|------|
| 1 | D3 | 重复的 extract_filename 函数 | network.cpp, pci.cpp, storage.cpp | 3 份相同代码，应提取到 parse_utils |
| 2 | D3 | 未使用参数处理不一致 | cpu.cpp:250 vs others | `[[maybe_unused]]` vs `(void)` cast |
| 3 | D4 | backend init/shutdown 异常安全 | `pipeline.cpp:118-159` | resolve 抛异常时 shutdown 不执行 |
| 4 | D6 | udevadm/EDAC/IfAddrs/df 畸形行静默跳过 | 多个 parser | 应发 warning 而非静默 continue |
| 5 | D6 | hostname 缺失无 warning | `platform.cpp:287-295` | 与 os-release/proc-version 不一致 |
| 6 | D6 | storage 分区匹配只取第一个 | `storage.cpp:218` | 多分区设备只记录第一个挂载点 |
| 7 | D7 | pci.cpp numa_node 死代码 | `pci.cpp:215-223` | parse_uint 不可解析 "-1"，分支不可达 |
| 8 | D7 | storage 分区前缀误匹配 | `storage.cpp:211` | "sda" 匹配 "sdab" |
| 9 | D2 | RawSource/CollectStatus 反序列化无 validate_enum | `serialize.cpp:118,121` | 其他枚举都有边界检查 |
| 10 | D1 | public_api.md `full | Collect::Raw` 示例矛盾 | `public_api.md:187-189` | 代码中 full 已含 Raw |
| 11 | D1 | memory.hpp 文件头注释未更新 | `memory.hpp:3` | 缺少 DimmInfo |
| 12 | D9 | test_replay CHECK(true) 占位 | `test_replay.cpp:61,92` | 无验证价值 |
| 13 | D9 | 新功能无畸形输入测试 | 多个 test 文件 | 只有 happy path 和 missing data |

---

## 版本趋势

| 维度 | v0.0.1 | v0.0.2 | v0.0.4 | 趋势 |
|------|--------|--------|--------|------|
| D1 设计文档忠实度 | 10 | 7 | 9 | ↓↑ — 文档对齐 commit 显著改善 |
| D2 API 优雅度 | 8 | 8 | 8 | — 稳定 |
| D3 代码一致性 | 8 | 8 | 8 | — 稳定 |
| D4 Resolver 正确性 | 5 | 7 | 7 | ↑— record_collector_status bug 预存 |
| D5 类型安全 | 9 | 8 | 7 | ↓↓ — 新字段类型安全退化 |
| D6 错误处理 | 7 | 8 | 7 | ↑↓ — 新 parser 静默失败路径增多 |
| D7 Parser 健壮性 | 7 | 7 | 8 | ↑— 边界处理改善 |
| D8 职责分离 | 9 | 8 | 9 | ↓↑ — syscall 正确归入 reader 层 |
| D9 测试质量 | 7 | 7 | 8 | ↑— 新功能场景覆盖好 |
| **加权总分** | **8** | **8** | **8** | **稳定** |

**总结**：v0.0.4 加权总分 8/10，与 v0.0.2 持平。改善领域（D1↑2, D7↑1, D8↑1, D9↑1）与退化领域（D5↓1, D6↓1）相互抵消。退化主要集中在 v0.0.4 新增字段和 parser 的类型安全/错误处理上——新代码功能正确但未完全遵循既有的强类型和 warning 惯例。P1 问题（7 项）应在发布前修复，P2 问题（13 项）可排入下个版本。

> **注**：本报告反映 v0.0.4 tag 时的代码状态。后续修复中 DimmInfo 已移除 `type`/`configured_speed_mts`（提升到 Memory 级别），`VirtualizationKind` 已新增 `Qemu`/`HyperV`/`VirtualBox`/`Parallels`。报告中引用的旧字段名和枚举值仅作历史记录。
