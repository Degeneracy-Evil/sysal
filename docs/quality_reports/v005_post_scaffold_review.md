# sysal v0.0.5 代码质量评审报告（脚手架迁移后）

## 评审日期

2026-07-31

## 评审版本

v0.0.5（脚手架迁移 + clang-format reformat 后）

## 变更范围（自上次评审）

| Commit | 说明 |
|--------|------|
| `fba7bf2` | refactor: 迁移 base_project 脚手架（check.sh → xmake check/test, autoupdate, 自包含 pre-commit） |
| `199e540` | style: clang-format reformat（ColumnLimit 120, PointerAlignment Right, NamespaceIndentation All） |
| `890e756` | docs: 标记所有 v0.0.2 计划为已完成，删除 set_version |

## 评分总览

| 维度 | v0.0.4 | v0.0.5 | v0.0.5' | 权重 | 加权 | 变化 | 说明 |
|------|:---:|:---:|:---:|:---:|:---:|:---:|------|
| D1 设计文档忠实度 | 9 | 8 | 8 | 15% | 1.20 | — | storage.md/raw_store.md 过期 |
| D2 API 优雅度 | 8 | 8 | 8 | 15% | 1.20 | — | 稳定 |
| D3 代码一致性 | 8 | 9 | 9 | 12% | 1.08 | — | clang-format 全量 reformat 后一致性好 |
| D4 核心逻辑正确性 | 7 | 8 | 8 | 12% | 0.96 | — | container env var 误分类 bug |
| D5 类型安全 | 7 | 8 | 8 | 8% | 0.64 | — | 稳定 |
| D6 错误处理健壮性 | 7 | 7 | 8 | 10% | 0.80 | ↑1 | clang-tidy 新检查零 warning，accelerator 兜底更健壮 |
| D7 Parser 健壮性 | 8 | 9 | 9 | 10% | 0.90 | — | 稳定 |
| D8 职责分离 | 9 | 9 | 9 | 10% | 0.90 | — | 层分离零违规 |
| D9 测试质量 | 8 | 8 | 8 | 8% | 0.64 | — | 稳定 |
| **加权总分** | **8** | **8** | **8** | **100%** | **8.32** | **—** | **稳定** |

加权总分计算：1.20 + 1.20 + 1.08 + 0.96 + 0.64 + 0.80 + 0.90 + 0.90 + 0.64 = 8.32 → **8**

---

## 各维度详评

### D1: 设计文档忠实度 8/10（—）

**亮点**：
- 所有 model 头文件与设计文档精确匹配（platform/cpu/memory/accelerator/storage/pci/network/software/execution）
- strong_typing.md 与 types/ 目录完全一致（StrongId 8 个、ScalarUnit 4 个、NamedString 8 个、enum 8 个）
- AGENTS.md 已更新，无对 check.sh/utils/ 的过时引用

**问题**：
- [WARN] `storage.md:19-20` 声明 `mount_point`/`fs_type` 为 `std::optional<std::string>`，代码已升级为 `std::optional<MountPoint>`/`std::optional<FilesystemType>`（强类型），文档滞后
- [WARN] `raw_store.md` 缺少 `SysHypervisor` 枚举项，且条目顺序与代码不一致

### D2: API 优雅度 8/10（—）

**亮点**：
- `System::collect(Collect flags = full)` 工厂 + `Collect` 位掩码组合直觉自然
- 查询方法返回 `const T*` / `vector<const T*>` 非拥有视图，零拷贝
- `to_json`/`from_json` 非侵入式自由函数，`from_json` 接受 `string_view`

**问题**：
- [WARN] `Collect` 缺少 `operator&`，无法写 `if (flags & Collect::Cpu)` 惯用模式
- [WARN] `refresh()` 无 `refresh(Collect flags)` 重载，改采集范围需销毁重建
- [WARN] `StrongId` 缺少 `operator<`，无法用于 `std::map`/`std::set`

### D3: 代码一致性 9/10（—）

**亮点**：
- clang-format 全量 reformat 后风格完全统一：4 空格缩进、NamespaceIndentation All、PointerAlignment Right、ColumnLimit 120
- 9 个 parser 命名、include 顺序、Doxygen 注释、错误处理模式零偏差
- `static_cast<unsigned char>()` 用于 `<cctype>` 函数，项目约定一致遵守

**问题**：
- [WARN] resolve.cpp 交叉校验警告用 `"[tag]"` 前缀，与 parser `"parse_<domain>:"` 前缀形成两套风格

### D4: 核心逻辑正确性 8/10（—）

**亮点**：
- 虚拟化检测优先级正确：sysfs → DMI → cpuinfo flags
- 可见性计算与交叉校验逻辑完备
- `infer_storage_kind` 分类链正确：nvme → virtual → rotational → default

**问题**：
- [FAIL] `execution.cpp:353-365`：`container` 环境变量非 docker 值一律归为 Podman，`container=lxc` 应映射 `Lxc`，`container=systemd-nspawn` 应映射 `Other`
- [WARN] 无云厂商 VM 检测（AWS EC2 "Amazon"/"EC2"、GCP "Google"）
- [WARN] `byte_order = "little"` 硬编码，大端架构（s390x、SPARC）会产出错误数据
- [WARN] ISA 扩展仅从第一个 cpuinfo 条目解析，假设同构 CPU

### D5: 类型安全 8/10（—）

**亮点**：
- 核心硬件模型（cpu/storage/network/pci/accelerator）ID/名称/厂商/单位字段全面使用 StrongId/NamedString/ScalarUnit
- 解析器通过显式构造产出强类型（`DeviceName{...}`、`Vendor{...}`、`MemorySize{...}`）

**问题**：
- [WARN] Platform 模型中 `hostname`/`machine_id`/`product_name`/`serial` 为裸 `std::string`
- [WARN] Software/Execution 模型中大量裸 `std::string`（驱动名/版本/路径等描述性字段）

### D6: 错误处理健壮性 8/10（↑1）

**亮点**：
- SysalError 仅在全部采集器失败时抛出，部分失败走 warnings 通道
- 各解析器对缺失/空/畸形数据均有 warning + nullopt 兜底
- warnings 消息包含上下文（原始值、字段名），有诊断价值
- clang-tidy 新增 4 项 modernize 检查零 warning 通过

**问题**：
- [WARN] `read_command` 未检查命令退出码非零的情况
- [WARN] `byte_order` 硬编码无 warning

### D7: Parser 健壮性 9/10（—）

**亮点**：
- 所有解析器一致检查缺失数据源（返回 nullopt + warning）
- Fallback 链：udevadm → EDAC（DIMM）、分区前缀匹配（storage mount）
- 枚举默认值：InterfaceState::Unknown、StorageKind::Other
- `parse_range_list` 有 MAX_IDS=1024 上限

**问题**：
- [WARN] `parse_os_release` 空 payload 不产生 warning
- [WARN] `read_command` 将空输出视为失败

### D8: 职责分离 9/10（—）

**亮点**：
- Reader 只写 RawStore，Parser 只读 RawStore 产出 model，Resolver 只组装 ParseResult，Pipeline 只编排
- 零跨层违规：无 parser 引用 reader 头文件，无 reader 构造 model 对象，无 resolver 做 I/O
- 依赖图严格单向，无循环依赖

**问题**：无

### D9: 测试质量 8/10（—）

**亮点**：
- 16 单元 + 1 集成测试覆盖全部 9 个域
- 解析测试使用真实格式的原始数据（/proc/cpuinfo、nvidia-smi CSV、lspci 输出）
- 边界测试覆盖：空输入、畸形数据、缺失可选字段、零值
- test_replay 真实数据回放：fixture → Parser → Resolver → 断言

**问题**：
- [WARN] 序列化 round-trip 仅抽查关键字段，非逐字段比较；新增模型字段若遗漏序列化不会被发现
- [WARN] 无畸形 JSON 反序列化测试（缺少必需键、类型错误、截断 payload）
- [WARN] 无 Reader 错误路径测试（权限拒绝、截断读取）
- [WARN] 无 System::collect 部分失败场景测试

---

## 关键问题优先级排序

### P1（应该修复）

| # | 维度 | 问题 | 位置 |
|---|------|------|------|
| 1 | D4 | `container` 环境变量非 docker 值误分类为 Podman | `execution.cpp:353-365` |
| 2 | D1 | `storage.md` mount_point/fs_type 类型声明过期 | `docs/design/data_model/storage.md:19-20` |
| 3 | D1 | `raw_store.md` 缺少 SysHypervisor 枚举项 | `docs/design/data_model/raw_store.md` |

### P2（建议修复，下个版本）

| # | 维度 | 问题 | 位置 |
|---|------|------|------|
| 4 | D2 | `Collect` 缺少 `operator&` | `include/sysal/core/collect.hpp` |
| 5 | D2 | `refresh()` 无 flags 重载 | `include/sysal/core/system.hpp:58` |
| 6 | D2 | `StrongId` 缺少 `operator<` | `include/sysal/types/strong_id.hpp` |
| 7 | D4 | 无云厂商 VM 检测（AWS/GCP） | `src/parser/platform.cpp` |
| 8 | D4 | `byte_order` 硬编码 "little" | `src/parser/platform.cpp:383` |
| 9 | D6 | `read_command` 未检查退出码 | `src/reader/linux/file_utils.hpp:59-63` |
| 10 | D9 | 序列化 round-trip 非逐字段比较 | `tests/unit/test_serialization.cpp` |
| 11 | D9 | 无畸形 JSON 反序列化测试 | `tests/unit/test_serialization.cpp` |

---

## 版本趋势

| 版本 | D1 | D2 | D3 | D4 | D5 | D6 | D7 | D8 | D9 | 总分 |
|------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| v0.0.2 | 7 | 8 | 8 | 7 | 8 | 8 | 7 | 8 | 7 | 8 |
| v0.0.4 | 9 | 8 | 8 | 7 | 7 | 7 | 8 | 9 | 8 | 8 |
| v0.0.5 | 8 | 8 | 9 | 8 | 8 | 7 | 9 | 9 | 8 | 8 |
| v0.0.5' | 8 | 8 | 9 | 8 | 8 | 8 | 9 | 9 | 8 | 8 |

D6 从 7 提升至 8（clang-tidy 新检查零 warning + accelerator 兜底健壮）。其余维度稳定。总分连续 4 版保持 8 分，质量基线稳固。
