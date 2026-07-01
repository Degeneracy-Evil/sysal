# 开发记录

### 2026-07-01 R2a: if-has(flags) 链重构为表驱动分派

- **变更类型**: src / refactor
- **涉及文件**: src/pipeline/pipeline.cpp, src/reader/linux/sysfs.cpp, src/reader/linux/procfs.cpp, docs/devlog.md
- **变更内容**:
  1. `pipeline.cpp`：9 个 `if(has(flags, Collect::Xxx))` 块替换为 `ParserDispatch` 表 + 循环。每个条目含 `Collect flag` 和函数指针（无捕获 lambda 通过 `+` 运算符转换为 `void(*)(ParseResult&, const RawStore&, vector<string>&)`），避免 `std::function` 开销
  2. `sysfs.cpp`：6 个 `if(has(flags, Collect::Xxx))` 块替换为 `ReaderDispatch` 表 + 循环。所有读取函数签名统一为 `void(*)(RawStore&)`，直接使用函数指针
  3. `procfs.cpp`：保留显式 if-has 块，添加注释说明原因——Cpu→Platform、Pci→Network、Software→Accelerator 三个跨域依赖使扁平表驱动分派不可行
- **原因**: 消除重复的 if-has 模式，提升可读性和可维护性；新增域只需在分发表添加一行
- **验证**: `xmake -r` 构建成功零 warning；`xmake run test_collect` 和 `xmake run test_replay` 全部通过

### 2026-07-01 抽象 resource.cpp 重复代码为模板辅助函数

- **变更类型**: src / refactor
- **涉及文件**: src/model/resource.cpp, docs/devlog.md
- **变更内容**:
  1. 新增匿名命名空间内两个模板辅助函数：`find_by_member`（按成员指针查找）和 `filter_by`（按谓词过滤）
  2. 将 6 个 `find_by_id` 模式函数（`Cpu::find_package`、`Cpu::find_core`、`Cpu::find_logical_cpu`、`Accelerators::find`、`Network::find`、`Pci::find`）替换为 `find_by_member` 单行调用
  3. 将 6 个 `filter_by_predicate` 模式函数（`Cpu::logical_cpus_of_package`、`Cpu::logical_cpus_of_core`、`Cpu::cores_of_package`、`Cpu::visible_logical_cpus`、`Accelerators::visible`、`Network::visible`）替换为 `filter_by` 单行调用
  4. 保留 `Accelerators::by_kind`/`gpus`/`npus`/`fpgas` 不变（已为委托链）
  5. 新增 `#include <type_traits>` 用于 `std::remove_reference_t`
  6. 文件从 200 行缩减至 144 行
- **原因**: resource.cpp ~80% 为重复的线性扫描模式，模板化消除冗余、提升可维护性
- **验证**: `xmake -r` 构建通过零 warning，`xmake run test_model` 全部通过，clang-tidy 零告警

### 2026-07-01 修复 4 个库 Bug（R1a/R1c/R1d/R1e）

- **变更类型**: src / fix
- **涉及文件**: src/parser/accelerator.cpp, src/reader/linux/file_utils.hpp, src/parser/platform.cpp, src/reader/linux/procfs.cpp, include/sysal/types/enums.hpp, tests/testbench.cpp, docs/devlog.md
- **变更内容**:
  1. R1a: `accelerator.cpp` — nvidia-smi CSV 字段 2/3 顺序修正：field[2] 为 memory.total，field[3] 为 pci.bus_id（原代码反了）
  2. R1c: `file_utils.hpp` — `read_command()` 追加 `2>/dev/null` 重定向 stderr，避免命令不存在时错误信息泄漏到终端
  3. R1d: `platform.cpp` — `kernel.version` 从 `/proc/version` 中提取 `#` 开头的构建版本标签（如 `#101-Ubuntu`），而非复制 `kernel.release`
  4. R1e: `enums.hpp` 新增 `ProcHostname` 枚举值；`procfs.cpp` 采集 `/proc/sys/kernel/hostname`；`platform.cpp` 解析 hostname 替换原警告占位
  5. `testbench.cpp` — switch 补充 `ProcHostname` 分支（-Wswitch 要求完整覆盖）
- **原因**: 4 个独立 Bug 修复，详见任务描述
- **验证**: `xmake -r` 构建通过，零 warning

### 2026-07-01 修复 R1 测试回归 + ProcHostname 枚举顺序

- **变更类型**: tests / fix
- **涉及文件**: tests/test_parse_accelerator.cpp, tests/test_parse_platform.cpp, include/sysal/types/enums.hpp, docs/devlog.md
- **变更内容**:
  1. `test_parse_accelerator.cpp` — 3 个测试 fixture 的 CSV 列顺序更新为 nvidia-smi 实际输出顺序 `index,name,memory.total,pci.bus_id,driver_version`
  2. `test_parse_platform.cpp` — `kernel.version` 断言从 `"5.15.0-91-generic"` 改为 `"#101-Ubuntu"`
  3. `enums.hpp` — `ProcHostname` 从 `ProcOneCgroup` 后移到枚举末尾，避免插入中间导致后续枚举值数字偏移、破坏已有 JSON fixture 的序列化兼容性
- **原因**: R1a/R1d 修复改变了行为，测试断言需同步更新；ProcHostname 插入中间导致 dev_machine.json fixture 中所有 source 数字偏移
- **验证**: 全部 20 个测试通过

### 2026-07-01 修复容器检测误报（bare metal 上报 Docker）

- **变更类型**: src / fix
- **涉及文件**: include/sysal/model/raw_store.hpp, src/model/raw_store.cpp, src/parser/execution.cpp, docs/devlog.md
- **变更内容**:
  1. `raw_store.hpp`：新增 `has_success(RawSource)` 声明，仅当记录状态为 `CollectStatus::Success` 时返回 true
  2. `raw_store.cpp`：实现 `has_success()`，使用 `std::ranges::any_of` 过滤 `source` 和 `status == Success`
  3. `execution.cpp`：`detect_container()` 中 `raw.has(RawSource::RootDockerenv)` 改为 `raw.has_success(RawSource::RootDockerenv)`
- **原因**: `has()` 不检查 `CollectStatus`，当 `/.dockerenv` 不存在时 reader 插入 `NotCollected` 记录，`has()` 仍返回 true，导致 bare metal 机器误报 Docker 容器
- **验证**: `xmake -r` 构建成功，零 warning

### 2026-07-01 集中版本管理 + GitHub Release v0.0.1

- **变更类型**: src / tests / build
- **涉及文件**: include/sysal/version.hpp (新增), src/pipeline/pipeline.cpp, src/serialization/serialize.cpp, tests/testbench.cpp, tests/test_collect.cpp, tests/test_serialization.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. 新建 `include/sysal/version.hpp`：`inline constexpr` 定义 `VERSION_MAJOR=0`、`VERSION_MINOR=0`、`VERSION_PATCH=1`、`VERSION_STRING="0.0.1"`
  2. `pipeline.cpp`：`meta.sysal_version = "0.0.1"` 改为 `sysal::VERSION_STRING`
  3. `serialize.cpp`：版本兼容性检查 `starts_with("0.0.")` 改为用 `VERSION_MAJOR`/`VERSION_MINOR` 动态构建前缀
  4. `testbench.cpp`、`test_collect.cpp`、`test_serialization.cpp`：版本断言改为 `sysal::VERSION_STRING`
  5. `xmake.lua`：添加 `set_version("0.0.1")`
- **原因**: 版本号从单点硬编码改为集中管理，便于版本升级时统一修改
- **验证**: `xmake -r` 构建成功；`xmake run test_collect` 通过；`xmake run test_serialization` 14 passed

### 2026-07-01 整理 xmake.lua 结构

- **变更类型**: build / refactor
- **涉及文件**: xmake.lua, docs/devlog.md
- **变更内容**:
  1. 删除 `after_build` 手写 compile_commands.json 生成（18 行），check.sh 已有 `xmake project -k compile_commands` 自动生成
  2. 合并 `test_target` 和 `test_target_shared` 为一个函数，通过 `link_shared` 布尔参数控制链接静态/动态库
  3. 19 个 test_target 调用改为循环 + 列表
  4. 四层视觉结构：全局配置 / 库 / 测试 / task，用 `==========` 分隔
  5. git hooks `on_load` 加注释说明为何挂在 `sysal_static` 上
  6. 静态库 `set_targetdir` 加注释说明 workaround 原因
- **原因**: 原 xmake.lua 有重复代码、职责混乱（项目级逻辑挂在 target 上）、冗余的手写 JSON 生成
- **验证**: `xmake -r` 构建成功；`xmake testbench` 运行正常；`xmake project -k compile_commands build` 生成 compile_commands.json

### 2026-07-01 添加 xmake testbench task + 移除 testbench 内部 include 路径

- **变更类型**: build / chore
- **涉及文件**: xmake.lua, docs/devlog.md
- **变更内容**:
  1. xmake.lua: 添加 `task("testbench")` 插件任务，`xmake testbench` 一键编译并运行 testbench，终端输出完整可见
  2. xmake.lua: `test_target_shared` 移除 `add_includedirs("src")`，testbench 不再能访问 `src/` 内部头文件，强制封装边界
- **原因**: testbench 需要便捷的运行方式查看终端输出；作为链接动态库的外部程序，testbench 不应访问库内部头文件
- **验证**: `xmake -r` 构建成功；`xmake testbench` 运行正常，终端输出 17 section 完整内容

### 2026-07-01 拆分静态库/动态库目标 + 重写 testbench 完整能力测试

- **变更类型**: build / tests / chore
- **涉及文件**: xmake.lua, utils/check.sh, tests/testbench.cpp, docs/devlog.md
- **变更内容**:
  1. xmake.lua: 原 `sysal` 目标拆分为 `sysal_static`（static）和 `sysal_shared`（shared），共享同一源文件列表 `SYSAL_SOURCES`；`set_basename("sysal")` 使输出为 `libsysal.a` / `libsysal.so`；静态库通过 `set_targetdir` 放到 `static/` 子目录避免链接器优先选择 `.so`
  2. xmake.lua: 新增 `test_target_shared` 辅助函数，testbench 改用 `test_target_shared` 链接 `sysal_shared`；19 个单元测试仍链接 `sysal_static`
  3. xmake.lua: `on_load`（git hooks）和 `after_build`（compile_commands.json）保留在 `sysal_static` 目标上
  4. utils/check.sh: 测试目标发现 grep 正则从 `test_target\("` 扩展为 `(?:test_target|test_target_shared)\("`，确保 testbench 被发现
  5. tests/testbench.cpp: 从 323 行打印 demo 重写为 856 行完整能力测试，覆盖 17 个 section：全量采集、Platform/CPU/Memory/Accelerators/Network/Storage/PCI/Software/Execution 各域详细输出与查询方法测试、可见性筛选、Raw Store、Warnings/Meta、JSON 序列化往返、部分采集、Refresh、错误处理（空 RawStore 抛 SysalError）
- **原因**: 作为库需要同时提供静态库和动态库两种构建目标；testbench 需完整测试所有公共 API 能力
- **验证**: `xmake -r` 构建成功（`libsysal.a` 在 `static/` 子目录 + `libsysal.so` 在主目录 + 全部测试）；`ldd test_types` 确认无动态依赖（静态链接）；`ldd testbench` 确认链接 `libsysal.so`；`utils/check.sh` 全部 4 项通过（20 测试）

### 2026-07-01 F3: 一致性修复（/// @file 头、include 风格、testbench 断言、NDEBUG 防护）

- **变更类型**: src / tests / build / chore
- **涉及文件**: src/parser/cpu.cpp, src/parser/execution.cpp, src/parser/platform.cpp, src/parser/accelerator.cpp, src/parser/memory.cpp, src/serialization/serialize.cpp, tests/test_types.cpp, tests/test_model.cpp, tests/test_raw_store_io.cpp, tests/test_reader.cpp, tests/test_replay.cpp, tests/testbench.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. F3a: 5 个 parser .cpp 文件添加 `/// @file` + `/// @brief` + `/// @details` Doxygen 头（cpu/execution/platform/accelerator/memory）
  2. F3b: 7 个文件中的 `#include <sysal/...>` 统一改为 `#include "sysal/..."`（serialize.cpp + 6 个测试文件）
  3. F3c: testbench.cpp 添加 `#include <cassert>` 和 9 个基本断言（collect 后 7 个 + refresh 后 2 个），使其既是 demo 也是有效测试
  4. F3e: xmake.lua 的 `test_target` helper 添加 `add_cxxflags("-UNDEBUG", {force = true})`，防止测试被 `-DNDEBUG` 编译时 `assert()` 被禁用
- **原因**: 代码一致性和测试质量问题——部分 .cpp 缺少 Doxygen 头、include 风格不统一、testbench 无断言、assert 可能被禁用
- **验证**: `utils/check.sh` 全部 4 项通过（clang-format + clang-tidy + build + tests）

### 2026-07-01 F2c: 4 个 parser 空数据时添加 warning

- **变更类型**: src / fix
- **涉及文件**: src/parser/network.cpp, src/parser/pci.cpp, src/parser/storage.cpp, src/parser/accelerator.cpp, docs/devlog.md
- **变更内容**: network/pci/storage/accelerator 4 个 parser 在源数据为空时静默返回 nullopt，现添加 warning 消息（`"parse_network: 缺少 SysfsNet 数据"` 等），与 cpu/memory/platform parser 保持一致
- **原因**: 4 个 parser 与 cpu/memory/platform 行为不一致，缺少 warning 不利于调试
- **验证**: `utils/check.sh` 全部 4 项通过

### 2026-07-01 F2: 修复 parse_uint/parse_hex 部分消费 + collect() 全失败抛异常

- **变更类型**: src / fix
- **涉及文件**: src/parser/parse_utils.cpp, tests/test_parse_utils.cpp, src/pipeline/pipeline.cpp, tests/test_collect.cpp, docs/devlog.md
- **变更内容**:
  1. `parse_uint` / `parse_hex` 新增 `ptr != trimmed.data() + trimmed.size()` 检查：`from_chars` 部分消费时返回 `nullopt`，拒绝 `"123abc"` / `"ffxyz"` 等输入
  2. `run_replay` 在 `record_collector_status` 之后检查全部请求采集器失败时抛出 `SysalError(ErrorKind::CollectionFailed, ...)`
  3. 新增测试：parse_uint/parse_hex 部分消费拒绝、空 RawStore 调用 collect_from_raw 抛 SysalError
- **原因**: `from_chars` 不要求消费全部输入，原实现静默接受部分消费导致错误值；设计文档要求全部采集器失败时抛 SysalError 但原实现未实现
- **验证**: `utils/check.sh` 全部通过（clang-format + clang-tidy + build + tests）

### 2026-07-01 F1a: 修复 read_cpufreq 忽略 package_id

- **变更类型**: src / fix
- **涉及文件**: src/parser/cpu.cpp, tests/test_parse_cpu.cpp, docs/devlog.md
- **变更内容**:
  1. 新增 `extract_cpu_number_from_path` 辅助函数：从 sysfs 路径（如 `cpu/cpu0/cpufreq/base_frequency`）中提取 CPU 编号
  2. `read_cpufreq` 签名修改：移除 `[[maybe_unused]]`，新增 `package_cpu_ids` 参数（该封装包含的逻辑 CPU 编号集合）
  3. `read_cpufreq` 逻辑修复：遍历 sysfs 记录时提取 CPU 编号，仅处理属于该封装的 CPU 记录。原实现对所有封装都取第一个 `base_frequency` 和 `scaling_max_freq`，导致多 socket 系统所有封装获得 cpu0 的频率
  4. 调用点修改：从 `entries` 收集每个封装的逻辑 CPU 编号集合，传入 `read_cpufreq`
  5. 新增测试 7：2 封装各 2 CPU，频率不同（package 0: 2400/3500 MHz，package 1: 1800/2900 MHz），断言各封装频率正确且互不相同
- **原因**: 多 socket 系统中所有封装获得相同频率（cpu0 的），违反设计意图
- **验证**: `utils/check.sh` 全部 4 项通过；`xmake run test_parse_cpu` 7 个测试全部通过

### 2026-07-01 F1b+F1c: 修复交叉校验同义反复 + 实现冲突解决框架

- **变更类型**: src / fix
- **涉及文件**: src/resolver/resolve.cpp, tests/test_resolve.cpp, docs/devlog.md
- **变更内容**:
  1. `cross_check_cpu_visibility` 重写：原实现比较 `visible_to_current_process` 与 `visible_logical_cpu_ids`（同义反复，compute 已从后者设置前者，永远一致）。新实现检测两类问题：(a) 幻影 ID——`visible_logical_cpu_ids` 引用模型中不存在的 CPU，格式 `[visibility_mismatch] cpu_N: in_visible_logical_cpu_ids but cpu does not exist in model`；(b) 约束提示——cpuset 限制可见 CPU 数量，格式 `[constraint] cpu visibility restricted: N total, M visible`
  2. `cross_check_accelerator_visibility` 同理重写：检测加速器幻影 ID 和环境变量约束提示
  3. 新增 `TrustLevel` 枚举（Backend=0 > Sysfs=1 > Procfs=2 > Command=3 > Inferred=4）和 `resolve_conflict` 辅助函数：当两来源值不同时，高信任（低数值）来源胜出，追加 `[conflict]` 格式警告；值相同时无冲突。v0.0.1 无多来源字段故未调用，框架已就绪
  4. `resolve()` 中冲突解决占位注释更新为框架就绪说明
  5. 测试 7 重写为幻影 ID 检测（CPU 99 不存在于模型）；新增测试 8（cpuset 约束提示 4 total 2 visible）；新增测试 9（加速器幻影 ID 检测）
- **原因**: F1b——原交叉校验是同义反复，无法发现真实问题（幻影 ID）；F1c——冲突解决仅有注释占位，需实现可测试的 helper
- **验证**: `utils/check.sh` 全部 4 项通过（clang-format + clang-tidy + build + tests）；`xmake run test_resolve` 9 个测试全部通过

### 2026-07-01 新增代码质量评审方法与首份评审报告

- **变更类型**: docs
- **涉及文件**: docs/code_quality_review_method.md, docs/quality_reports/v001_initial_rewrite.md
- **变更内容**:
  1. 新增 `docs/code_quality_review_method.md`：定义可复用的多维度代码质量评审方法（9 个维度、权重分配、Oracle agent 分批执行流程、prompt 模板、汇总报告格式）
  2. 新增 `docs/quality_reports/v001_initial_rewrite.md`：v0.0.1 重写后的首份质量评审报告（加权总分 8/10，9 维度详评，11 个关键问题按 P0/P1/P2 排序）
- **原因**: 建立可复用的代码质量评价体系，便于后续开发周期重复执行；记录首次评审基线供后续对比
- **验证**: 方法文档与报告文档分离存放；评审由 5 个 Oracle agent 分 3 批并行执行

### 2026-06-30 Phase 10: 测试基础设施与重写收尾

- **变更类型**: src / build / docs
- **涉及文件**: tests/test_replay.cpp, tests/testbench.cpp, tests/fixtures/, xmake.lua, docs/issues.md, docs/devlog.md
- **变更内容**:
  1. `tests/test_replay.cpp`：raw replay 测试。首次运行时自动采集当前机器原始数据并保存到 `tests/fixtures/dev_machine.json`；后续运行加载 fixture 执行 Parser→Resolver 回放管线，验证域不变量（CPU 非空、内存 > 0、平台信息非空、网络/PCI 非空、执行上下文有效、元数据完整），并与实时采集对比关键指标（CPU 数量、内存总量）
  2. `tests/testbench.cpp`：全量 API 演示。采集当前机器全部系统信息，以格式化文本输出各子域（Platform/CPU/Memory/Accelerator/Network/PCI/Storage/Software/Execution），演示 JSON 序列化（pretty print）与 refresh 功能。PCI 地址使用十六进制零填充格式（`%04x:%02x:%02x.%x`），速率输出 Mbps，内存输出 GiB/MiB
  3. `tests/fixtures/`：fixture 目录，存放 raw replay 测试的原始数据快照
  4. `xmake.lua`：新增 `test_replay` 和 `testbench` 两个测试目标
  5. `docs/issues.md`：更新全部 18 个已知问题的状态——14 个已通过重写修复、2 个部分修复（B-2 NVMe symlink 链、D-8 软件栈大面积空白）、2 个已移除（D-3 TopologyInfo、D-5 PciRelation）。移除旧类型名引用（TopologyInfo、Diagnostics、PlatformInfo）
  6. `docs/devlog.md`：本条目
- **原因**: Phase 10 重写收尾——创建最终测试基础设施，更新文档反映重写成果
- **验证**: `xmake -r` 构建成功，`xmake run test_replay` 全部通过，`xmake run testbench` 输出正常，`utils/check.sh` 全部通过

### 2026-06-30 重写总结（Phase 1–10）

sysal v0.0.1 重写完成。10 个阶段的核心变更：

| Phase | 内容 | 关键产出 |
|-------|------|----------|
| 1 | 设计文档重写 | 19 个设计文档，反映新架构 |
| 2 | 数据模型 | 11 个 model 头文件，System/SystemInfo/Collect |
| 3 | 类型系统 | StrongId/NamedString/ScalarUnit 模板，消除重复 |
| 4 | JSON 引擎 | 纯 JSON 解析/发射，无 sysal 耦合 |
| 5 | RawStore 序列化 | save/load_raw_store，测试基础设施 |
| 6 | Linux Reader | procfs + sysfs 采集器，C-1 修复 |
| 7 | 9 个域 Parser | platform/cpu/memory/accelerator/storage/pci/network/software/execution |
| 8 | Resolver + Pipeline + System API | collect/refresh/collect_from_raw，可见性计算 |
| 9 | System JSON 序列化 | to_json/from_json，版本兼容 |
| 10 | 测试基础设施 | test_replay + testbench + fixture + 文档更新 |

修复的已知问题：A-1/A-2/A-3、B-1/B-3、C-1/C-2/C-3/C-4、D-1/D-2/D-4/D-6/D-7、E-1/E-2/E-3、F-1/F-2（共 16 个）。部分修复：B-2、D-8。已移除：D-3、D-5。

### 2026-06-30 Phase 9: System JSON 序列化（to_json / from_json）

- **变更类型**: src / build
- **涉及文件**: include/sysal/serialization/serialization.hpp, src/serialization/serialize.cpp, tests/test_serialization.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. `serialization.hpp`：新增 SerializationOptions 结构体（pretty_print、include_raw、include_meta）、to_json / from_json 自由函数声明
  2. `serialize.cpp`：在已有 RawStore 序列化基础上扩展 System↔JSON 实现。to_json 序列化 info（9 个子域）、meta（可选）、warnings、raw（可选）；from_json 解析全部字段并做版本兼容性检查（0.0.x）。为每个子结构体实现独立的 to/from 辅助函数，处理强类型（StrongId::value()、NamedString::value、ScalarUnit::value、PciAddress 对象、enum static_cast）、可选字段（firmware、virtualization、cuda、rocm、level_zero、mpi、rdma、container 等）
  3. `test_serialization.cpp`：7 个测试（round-trip 往返、include_raw=false 无 raw 键、include_raw=true 有 raw 键、include_meta=false 无 meta 键、include_meta=true 有 meta 键、版本不兼容抛 SysalError、兼容版本正常解析）
  4. `xmake.lua`：新增 test_serialization 测试目标
- **原因**: Phase 9 实现——System JSON 序列化与反序列化，支持 raw replay 测试管线
- **验证**: xmake -r 成功，xmake run test_serialization 14 项全部通过

### 2026-06-30 Phase 8: Resolver、Pipeline、System::collect/refresh

- **变更类型**: src / build
- **涉及文件**: src/resolver/resolve.hpp, src/resolver/resolve.cpp, src/pipeline/pipeline.hpp, src/pipeline/pipeline.cpp, src/api/system.cpp, include/sysal/test/replay.hpp, tests/test_resolve.cpp, tests/test_collect.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. `resolve.hpp` + `resolve.cpp`：实现 `resolve(ParseResult, warnings) → SystemInfo`。移动 ParseResult 各 optional 域到 SystemInfo（缺省默认构造）；计算 CPU 可见性（cpuset 约束）、加速器可见性（CUDA_VISIBLE_DEVICES）、网络接口可见性（v0.0.1 全部可见）；交叉校验资源级 visible_to_current_process 与 ExecutionContext 便利索引的一致性，不一致时追加 `[visibility_mismatch]` 警告；冲突解决框架就绪（v0.0.1 大多单来源，格式 `[conflict] <field>: <src1>=<val>, <src2>=<val>, adopted=<src>`）
  2. `pipeline.hpp` + `pipeline.cpp`：实现 `run_pipeline(flags, warnings)` 和 `run_replay(raw, flags, warnings)`。run_pipeline 执行 Reader→Parser→Resolver 完整管线；run_replay 从已有 RawStore 执行 Parser→Resolver 回放管线；按域调用 9 个解析器；记录成功/失败采集器；构建 SnapshotMeta（collect_time、sysal_version="0.0.1"、collect_duration、requested_flags）；后端 init/shutdown 生命周期占位；实现 `sysal::test::collect_from_raw` 公共接口
  3. `system.cpp`：实现 `System::collect(flags)` 委托 `run_pipeline`，`System::refresh()` 用 `meta.requested_flags` 重新采集并移动赋值
  4. `replay.hpp`：新增 `collect_from_raw(raw, flags)` 声明
  5. `test_resolve.cpp`：7 个测试（CPU 可见性 cpuset 约束、CPU 无约束全可见、加速器可见性、加速器无约束、网络接口全可见、缺失域默认构造、交叉校验一致性）
  6. `test_collect.cpp`：3 个测试（collect 冒烟、refresh 保持一致、元数据正确）
  7. `xmake.lua`：新增 test_resolve、test_collect 两个测试目标
- **原因**: Phase 8 核心实现——Resolver 冲突解决与可见性计算、Pipeline 管线编排、System 公共 API
- **验证**: `utils/check.sh` 全部 4 项检查通过（clang-format + clang-tidy + build + tests）

### 2026-06-30 Phase 7: Software、Execution 域解析器

- **变更类型**: src / build
- **涉及文件**: src/parser/software.hpp, src/parser/software.cpp, src/parser/execution.hpp, src/parser/execution.cpp, tests/test_parse_software.cpp, tests/test_parse_execution.cpp, xmake.lua
- **变更内容**: 新增 software 和 execution 两个域解析器。software 解析 NVIDIA 驱动版本（nvidia-smi）和 CUDA 版本（nvcc --version），构建 SoftwareStack。execution 解析 /proc/self/status（进程、权限、cpuset）、/proc/self/cgroup（cgroup v1/v2）、环境变量、容器检测，构建 ExecutionContext。两个解析器均遵循 parse_<domain>(const RawStore&, warnings) → optional<T> 接口，不调用任何 syscall。
- **原因**: Phase 7 最后两个域解析器，完成全部 10 个域的解析器实现
- **验证**: xmake -r 成功，xmake run test_parse_software 和 test_parse_execution 通过，utils/check.sh 全部 4 项检查通过

### 2026-06-30 Phase 7: Accelerator、Storage 域解析器

- **变更类型**: src / build
- **涉及文件**: src/parser/accelerator.hpp, src/parser/accelerator.cpp, src/parser/storage.hpp, src/parser/storage.cpp, tests/test_parse_accelerator.cpp, tests/test_parse_storage.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. `accelerator.hpp` + `accelerator.cpp`：实现 `parse_accelerator(RawStore&, warnings)` → `std::optional<Accelerators>`。解析 NvidiaSmi CSV 输出（index/name/pci.bus_id/memory.total）→ AcceleratorDevice 列表，支持 MiB/GiB/KiB 单位转换，SysfsPci numa_node 查找（D-4 修正）
  2. `storage.hpp` + `storage.cpp`：实现 `parse_storage(RawStore&, warnings)` → `std::optional<Storage>`。解析 SysfsBlock 记录（按设备名分组，size×512→容量），设备名前缀推断 StorageKind（nvme→Nvme, sd→Sata），B-2 修正：PCI 地址暂缺并发出警告
  3. `test_parse_accelerator.cpp`：4 个测试（2 GPU 解析、NUMA 查找、空数据 nullopt、GiB 单位）
  4. `test_parse_storage.cpp`：4 个测试（nvme+sda 解析、空数据 nullopt、Other 类型推断、B-2 警告）
  5. `xmake.lua`：新增 test_parse_accelerator、test_parse_storage 两个测试目标
- **原因**: Phase 7 域解析器实现，accelerator 和 storage 两个子域
- **验证**: `utils/check.sh` 全部通过（clang-format + clang-tidy + build + tests）

### 2026-06-30 Phase 7: PCI、Network 域解析器

- **变更类型**: src / build
- **涉及文件**: src/parser/pci.hpp, src/parser/pci.cpp, src/parser/network.hpp, src/parser/network.cpp, tests/test_parse_pci.cpp, tests/test_parse_network.cpp, xmake.lua
- **变更内容**: 新增 PCI 和 Network 域解析器，从 RawStore 中解析 Pci/Network 结构体；PCI 解析器从 SysfsPci 记录提取地址、厂商、设备名、类别、NUMA 节点；Network 解析器从 SysfsNet 记录提取接口名、MAC、链路状态、速率；新增对应测试；xmake.lua 添加 test_parse_pci 和 test_parse_network 目标
- **原因**: Phase 7 域解析器扩展，覆盖 PCI 和 Network 子系统
- **验证**: `utils/check.sh` 全量通过（clang-format + clang-tidy + build + tests）

### 2026-06-30 Phase 7: Platform、CPU、Memory 域解析器

- **变更类型**: src / build
- **涉及文件**: src/parser/platform.hpp, src/parser/platform.cpp, src/parser/cpu.hpp, src/parser/cpu.cpp, src/parser/memory.hpp, src/parser/memory.cpp, tests/test_parse_platform.cpp, tests/test_parse_cpu.cpp, tests/test_parse_memory.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. `platform.hpp` + `platform.cpp`：实现 `parse_platform(RawStore&, warnings)` → `std::optional<Platform>`。解析 EtcOsRelease（os-release 键值对）→ Os，ProcVersion（内核发行号、编译时间）→ Kernel，Uname（架构名称、位宽、字节序）→ Architecture，SysfsDmi（BIOS 厂商/版本/日期、产品名/厂商/序列号）→ Firmware + Host，ProcOneCgroup + RootDockerenv（Docker/KVM 虚拟化检测）→ Virtualization
  2. `cpu.hpp` + `cpu.cpp`：实现 `parse_cpu(RawStore&, warnings)` → `std::optional<Cpu>`。解析 ProcCpuInfo（逐行解析，空行分隔条目）→ CpuPackage/CpuCore/LogicalCpu 拓扑，flags → IsaExtension 列表，SysfsCpu（cpufreq base_frequency/scaling_max_freq）→ 频率，SysfsNuma（cpulist 范围解析）→ NumaNode 映射
  3. `memory.hpp` + `memory.cpp`：实现 `parse_memory(RawStore&, warnings)` → `std::optional<Memory>`。解析 ProcMemInfo（MemTotal/MemAvailable kB→bytes）→ 总量/可用，SysfsNuma（nodeN/meminfo）→ NumaMemory 列表
  4. 三个测试文件：手造 RawStore 载荷，断言结构化字段（6 组平台测试、6 组 CPU 测试、5 组内存测试）
  5. xmake.lua 新增 test_parse_platform、test_parse_cpu、test_parse_memory 三个测试目标
- **原因**: Phase 7 要求实现 Parser 层，将 RawStore 原始证据解析为强类型模型
- **验证**: `utils/check.sh` 全部通过（clang-format + clang-tidy + build + 9 tests）

### 2026-06-30 Phase 6: Linux procfs 与 sysfs 采集器

- **变更类型**: src / build
- **涉及文件**: src/reader/linux/file_utils.hpp, src/reader/linux/procfs.hpp, src/reader/linux/procfs.cpp, src/reader/linux/sysfs.hpp, src/reader/linux/sysfs.cpp, tests/test_reader.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. `file_utils.hpp`：header-only 工具库，提供 `read_file`（读取文件全部内容）、`read_command`（popen 执行命令并读取标准输出）、`file_exists`（检查文件是否存在）、`add_record`（向 RawStore 添加 RawRecord 便利函数）
  2. `procfs.hpp` + `procfs.cpp`：实现 `read_procfs(RawStore&, Collect flags)`，按 Collect 位掩码采集：Platform 域（/proc/cpuinfo、/proc/version、/etc/os-release、uname -m、/.dockerenv）、Memory 域（/proc/meminfo）、Accelerator 域（nvidia-smi、nvcc）、Network 域（lspci）、Storage 域（lsblk）、Pci 域（lspci 补充）、Software 域（nvidia-smi/nvcc 补充）、Execution 域（/proc/self/cgroup、/proc/self/status、/proc/1/cgroup、7 个环境变量）。跨域共享来源（如 ProcCpuInfo）仅采集一次
  3. `sysfs.hpp` + `sysfs.cpp`：实现 `read_sysfs(RawStore&, Collect flags)`，按 Collect 位掩码采集：Cpu 域（遍历 /sys/devices/system/cpu/cpuN，读取 topology/physical_package_id、topology/core_id、online、cpufreq/base_frequency、cpufreq/scaling_max_freq）、Memory 域（遍历 /sys/devices/system/node/nodeN，读取 cpulist、meminfo）、Network 域（遍历 /sys/class/net，读取 address、operstate、speed）、Pci 域（遍历 /sys/bus/pci/devices，读取 vendor、device、class、numa_node）、Storage 域（遍历 /sys/block，读取 size、device/model）、Platform 域（/sys/class/dmi/id 下 6 个 DMI 文件）
  4. `tests/test_reader.cpp`：file_utils 测试（read_file 成功/失败、file_exists）、procfs 采集测试（10 个 RawSource 断言）、sysfs 采集测试（6 个 RawSource 断言）、合并采集测试（记录数与采集状态验证）
  5. xmake.lua 新增 `test_target("test_reader", "tests/test_reader.cpp")`
- **原因**: Phase 6 重写计划要求实现 Reader 层，将所有原始数据采集到 RawStore，修复 C-1 bug（parser 不直接调用 syscall，而是从 Reader 采集的 /proc/self/status 中解析 PID/UID/GID）
- **验证**: `utils/check.sh` 全部 4 项通过（clang-format + clang-tidy + build + tests）；`xmake run test_reader` 全部断言通过

### 2026-06-30 Phase 5: RawStore JSON 序列化与 save/load 测试基础设施

- **变更类型**: src / build
- **涉及文件**: include/sysal/test/replay.hpp, src/serialization/serialize.cpp, tests/test_raw_store_io.cpp, xmake.lua, src/reader/linux/file_utils.hpp, src/reader/linux/procfs.cpp, src/reader/linux/sysfs.cpp, docs/devlog.md
- **变更内容**:
  1. `include/sysal/test/replay.hpp`：声明 `load_raw_store` 和 `save_raw_store`（`collect_from_raw` 留待 Phase 8）
  2. `src/serialization/serialize.cpp`：实现 RawStore ↔ JSON 序列化。JSON 格式为顶层对象含 `records` 数组，每条记录含 `source`（RawSource 整数值）、`path_or_command`（字符串）、`payload`（字符串）、`status`（CollectStatus 整数值）、`collected_at`（epoch 毫秒）。实现 `save_raw_store`（写 JSON 到文件）和 `load_raw_store`（读文件并解析为 RawStore），失败时抛 `SysalError`
  3. `tests/test_raw_store_io.cpp`：4 组测试——往返一致性（3 条不同来源记录 save→load→比较）、加载不存在文件抛 SysalError(FileNotFound)、加载畸形 JSON 抛 SysalError(DeserializationError)、空 RawStore 往返
  4. xmake.lua 新增 `test_target("test_raw_store_io", "tests/test_raw_store_io.cpp")`
  5. 修复预存文件的 clang-format 和 clang-tidy 问题（file_utils.hpp/procfs.cpp/sysfs.cpp 格式化，sysfs.cpp 中 `auto dir = entry.path()` 改为 `const auto& dir = entry.path()` 消除 unnecessary-copy-initialization 警告）
- **原因**: Phase 5 重写计划要求实现 RawStore JSON 序列化与 save/load 测试基础设施，为 raw replay 测试策略提供持久化能力
- **验证**: `utils/check.sh` 全部 4 项通过（clang-format + clang-tidy + build + tests）；`xmake run test_raw_store_io` 4/4 通过

### 2026-06-30 Phase 2: 数据模型头文件与实现

- **变更类型**: src
- **涉及文件**: include/sysal/model/platform.hpp, include/sysal/model/cpu.hpp, include/sysal/model/memory.hpp, include/sysal/model/accelerator.hpp, include/sysal/model/network.hpp, include/sysal/model/storage.hpp, include/sysal/model/pci.hpp, include/sysal/model/software.hpp, include/sysal/model/execution.hpp, include/sysal/model/raw_store.hpp, include/sysal/model/system_info.hpp, include/sysal/core/system.hpp, include/sysal/core/sysal.hpp, src/model/raw_store.cpp, src/model/resource.cpp, tests/test_model.cpp, xmake.lua
- **变更内容**:
  1. 创建 11 个 model 头文件：platform/cpu/memory/accelerator/network/storage/pci/software/execution/raw_store/system_info
  2. 创建 2 个 core 头文件：system.hpp（System 类）、sysal.hpp（总入口）
  3. 实现 raw_store.cpp：get_all/get/has/count 四个查询方法
  4. 实现 resource.cpp：Cpu 7 个查询方法、Accelerators 6 个查询方法、Network 2 个查询方法、Pci 1 个查询方法
  5. 创建 test_model.cpp：Cpu/Accelerators/Pci/RawStore 查询方法测试
  6. xmake.lua 新增 test_model 测试目标
- **原因**: Phase 2 重写计划要求创建全部数据模型头文件与实现文件
- **验证**: `utils/check.sh` 全部通过（clang-format + clang-tidy + build + tests）

### 2026-06-30 Phase 4: 提取纯 JSON 引擎

- **变更类型**: src
- **涉及文件**: src/serialization/json.hpp, tests/test_json.cpp, xmake.lua
- **变更内容**:
  1. 从旧 `src/detail/json.hpp`（git HEAD~2）提取纯 JSON 部分，创建 `src/serialization/json.hpp`
  2. 移除所有 sysal 头文件依赖，移除 `raw_store_to_json`/`raw_store_from_json` 函数
  3. 用 `JsonError`（继承 `std::exception`）替代 `SysalError`/`Expected` 错误处理
  4. 新增 `dump_json()` 函数，将 `JsonVal` 发射为 JSON 文本
  5. 保留 `escape_string`、`JsonObj`、`JsonArr`、`JsonVal`、`JsonParser`、`parse_json`、`time_point_to_ms`、`ms_to_time_point`
  6. 创建 `tests/test_json.cpp`：基本类型、字符串转义、容器、嵌套、dump、往返一致性、escape_string、时间工具、错误处理、构建器共 10 组测试
  7. xmake.lua：`test_target` 辅助函数增加 `add_includedirs("src")`，新增 `test_target("test_json", "tests/test_json.cpp")`
- **原因**: Phase 4 重写计划要求将手写 JSON 引擎从旧代码中提取为独立纯 JSON 库，消除 sysal 类型耦合
- **验证**: `utils/check.sh` 全部通过（clang-format + clang-tidy + build + tests）

### 2026-06-30 修复设计文档遗漏（pipeline + raw_store）

- **变更类型**: docs
- **涉及文件**: docs/design/architecture/pipeline.md, docs/design/data_model/raw_store.md
- **变更内容**:
  1. pipeline.md：parser 目录列表补上遗漏的 `platform.hpp / platform.cpp`（ParseResult 有 9 个 optional 字段含 platform，但目录列表只列了 8 个 parser）
  2. raw_store.md：`RawSource` 枚举从 11 个值扩展到 23 个，按来源分组（procfs / sysfs / 文件命令 / 环境变量 / 未来后端），补上 C-1 修复（parser 不直接调用 syscall）和数据空白修复（D-1/D-2/D-4/D-6/D-7）所需的全部来源
- **原因**: 重写计划审计阶段发现 pipeline.md 的 parser 目录遗漏 platform parser；raw_store.md 的 RawSource 枚举缺少实现所需的来源值（ProcSelfStatus/ProcSelfCgroup/ProcOneCgroup 用于 execution parser 的 C-1 修复，SysfsNuma 用于 CPU NUMA 归属和内存 NUMA 分布，SysfsDmi 用于固件信息，EtcOsRelease/Uname 用于 platform parser，Environment 用于环境变量采集，RootDockerenv 用于容器检测，Nvcc 用于 CUDA 版本检测，SysfsBlock 用于存储设备）
- **验证**: 文档审查，确认 pipeline.md parser 目录列表与 ParseResult 的 9 个字段一一对应；raw_store.md 的 RawSource 枚举覆盖全部 parser 所需的来源

### 2026-06-30 从 resource_info.md 拆分 6 个子系统设计文档

- **变更类型**: docs
- **涉及文件**: docs/design/data_model/cpu.md, docs/design/data_model/memory.md, docs/design/data_model/accelerator.md, docs/design/data_model/network.md, docs/design/data_model/pci.md, docs/design/data_model/storage.md
- **变更内容**:
  1. 将原 resource_info.md 设计文档按子系统拆分为 6 个独立文件，每个文件聚焦一个子系统
  2. 按新命名规则重命名聚合类型：`CpuSubsystem` → `Cpu`、`MemorySubsystem` → `Memory`、`AcceleratorSubsystem` → `Accelerators`、`NetworkSubsystem` → `Network`、`StorageSubsystem` → `Storage`、`PciSubsystem` → `Pci`、`NumaMemoryInfo` → `NumaMemory`
  3. cpu.md：包含 StrongId 类型定义（CpuPackageId/CpuCoreId/LogicalCpuId）、CpuPackage/CpuCore/LogicalCpu/Cpu 结构体及便利查询方法；说明 `LogicalCpu::package_id` 反范式化与 `numa_node` 直接从 sysfs 读取
  4. memory.md：Memory 与 NumaMemory 结构体
  5. accelerator.md：AcceleratorKind 枚举、AcceleratorDevice 与 Accelerators 结构体；说明 `devices` 是数据真源、便利方法为非持有型过滤、`nearest_numa_node` 直接从 sysfs 读取
  6. network.md：NetworkInterface 与 Network 结构体
  7. pci.md：PciDevice 与 Pci 结构体；说明 Pci 是设备清单、`numa_node` 直接从 sysfs 读取
  8. storage.md：StorageDevice 与 Storage 结构体；说明 v0.0.1 仅提供基本设备清单
  9. 不包含 `ResourceInfo` 聚合类型（已移除，SystemInfo 直接包含各子系统）
- **原因**: 拆分单一大文档为聚焦的子系统文档，便于维护；统一去除 `Info`/`Subsystem` 后缀，简化类型命名
- **验证**: 文档审查，确认无 `ResourceInfo`/`CpuSubsystem`/`MemorySubsystem`/`AcceleratorSubsystem`/`NetworkSubsystem`/`StorageSubsystem`/`PciSubsystem`/`NumaMemoryInfo` 等带后缀类型名残留；代码块类型名与 ids.hpp/enums.hpp/resource_info.hpp 一致

### 2026-06-30 重写 7 个数据模型设计文档以反映新架构

- **变更类型**: docs
- **涉及文件**: docs/design/data_model/system_snapshot.md, docs/design/data_model/platform_info.md, docs/design/data_model/resource_info.md, docs/design/data_model/software_stack_info.md, docs/design/data_model/execution_context.md, docs/design/data_model/raw_store.md, docs/design/data_model/diagnostics.md
- **变更内容**:
  1. system_snapshot.md：`SystemSnapshot` 改为 `std::vector<std::string> warnings` 替代 `Diagnostics`，`ResourceInfo` 不含 topology；新增 `System` 类说明（对象持有模式，构造时抛 `SysalError`，构造后不可变）；`SnapshotMeta::requested_spec` 改为 `requested_flags`（类型 `Collect`）
  2. platform_info.md：保持内容，确认中文
  3. resource_info.md：`ResourceInfo` 移除 `TopologyInfo topology` 字段；保留 `CpuCore::numa_node`、`LogicalCpu::numa_node`、`AcceleratorDevice::nearest_numa_node`、`PciDevice::numa_node`，注释改为"从 sysfs 直接读取"；移除所有 TopologyInfo 引用与"PciSubsystem 是清单 / TopologyInfo 是关系图"对比
  4. software_stack_info.md：保持内容，确认中文
  5. execution_context.md：保持内容，确认无拓扑引用
  6. raw_store.md：`RawSource` 移除 `HwlocXml`，保留其余值；说明 `RawStore` 在 `SystemSnapshot` 中可选，通过 `Collect::Raw` 启用（替代 `CollectSpec::with_raw()`）
  7. diagnostics.md：标题改为 Warnings（警告信息），移除 `Severity`/`ConflictDetail`/`Diagnostic`/`Diagnostics` 结构体，改为描述 `std::vector<std::string> warnings`，保留简化示例
- **原因**: 反映 sysal 架构变更（System 类替代 collect/collect_or_throw、Collect 位掩码替代 CollectSpec、移除 Expected/SysalError 抛出、移除 TopologyInfo 等拓扑结构、Diagnostics 简化为 warnings、RawSource 移除 HwlocXml）
- **验证**: 文档审查，确认无 TopologyInfo/NumaRelation/PciRelation/DeviceLocality/Diagnostics/ConflictDetail/Severity/Expected/CollectSpec/HwlocXml 残留

### 2026-06-30 重写 5 个架构与规则设计文档以反映新架构

- **变更类型**: docs
- **涉及文件**: docs/design/architecture/pipeline.md, docs/design/architecture/backend_strategy.md, docs/design/rules/strong_typing.md, docs/design/rules/conflict_resolution.md, docs/design/rules/thread_safety.md
- **变更内容**:
  1. pipeline.md：管线改为 `Reader → RawStore → Parser → ParseResult → Resolver → System`；`ParsedFacts` 重命名为 `ParseResult`，字段改为公共类型，移除 topology 字段；更新源码布局为新命名（`cpu.hpp`、`procfs.hpp`、`resolve.hpp`、`system.cpp` 等），移除 `backend/` 目录
  2. backend_strategy.md：移除 hwloc 与 Topology 行，更新后端策略为 procfs+sysfs+PCI / NVML / ROCm SMI / Level Zero / ibverbs，说明 NUMA 设备级 `numa_node` 从 sysfs 读取
  3. strong_typing.md：保持不变（无 TopologyInfo 引用），确认中文
  4. conflict_resolution.md：来源信任顺序移除 hwloc，移除 `ConflictDetail`/`Diagnostics`，冲突改为字符串形式记录到 `System::warnings()`
  5. thread_safety.md：更新为对象持有模式（`System::collect()`、`System` 对象、`System::refresh()`、内部组件），实现约束改为 System 对象构造后只提供 const 访问
- **原因**: 反映 sysal 架构变更（System 类替代双入口 API、Collect 位掩码、移除 Expected/Diagnostics/TopologyInfo/hwloc、ParseResult 重命名、文件命名调整）
- **验证**: 文档审查，确认无拓扑/hwloc/Diagnostics/Expected/CollectSpec 及旧文件命名残留

### 2026-06-20 从 base_project 迁移脚手架

- **变更类型**: chore / build / docs
- **涉及文件**: xmake.lua, .clang-format, .clang-tidy, .clangd, .gitignore, .githooks/pre-commit, utils/check.sh, AGENTS.md, docs/devlog.md, include/sysal/sysal.hpp, src/sysal.cpp
- **变更内容**: 从 base_project 迁移工程基础设施模板，包括 xmake 构建配置（C++23/clang/libc++/lld）、clang-format 格式化规则、clang-tidy 静态分析配置、clangd IDE 集成、git hooks（pre-commit 转发到 check.sh）、统一质量检查脚本（hook/全量双模式）。适配为 sysal 静态库目标，创建 include/sysal/ 和 src/ 目录结构及占位文件。check.sh 文件收集改为 find 递归以适配子目录布局。
- **原因**: sysal 项目起步，需要一套完整的 C++ 工程基础设施
- **验证**: `xmake build` 通过，`utils/check.sh` 全量检查通过

### 2026-06-20 同步 base_project 模板更新

- **变更类型**: chore / build / docs / ci
- **涉及文件**: .gitattributes, .clang-tidy, xmake.lua, .github/workflows/ci.yml, tests/.gitkeep, AGENTS.md, README.md, docs/devlog.md
- **变更内容**:
  1. 创建 `.gitattributes`，统一行尾为 LF
  2. `.clang-tidy` 设置 `WarningsAsErrors: '*'`，静态分析零容忍
  3. `xmake.lua` 添加 `after_build` 钩子，构建后自动生成 `compile_commands.json`（手动构造 JSON，避免 xmake 递归调用死锁）
  4. 创建 `.github/workflows/ci.yml`，push/PR 时自动安装工具链并运行 `utils/check.sh`
  5. 添加 `tests/.gitkeep`，确保空目录被 git 跟踪
  6. AGENTS.md / README.md 更新约定描述：前置条件表格、CI 说明、自动 compile_commands、clang-tidy 零容忍、.gitattributes
- **原因**: 与 base_project 模板保持同步，提升开箱即用性和 CI 就绪度
- **验证**: `xmake -r` 后 `compile_commands.json` 自动生成，`utils/check.sh` 4/4 通过

### 2026-06-20 撰写设计修订提案

- **变更类型**: docs
- **涉及文件**: docs/design_proposals.md, docs/devlog.md
- **变更内容**: 针对 design_document.md 初版识别出的 14 项架构问题，逐项撰写详细设计提案，涵盖公共 API 统一（CollectSpec builder）、CPU 层级关系（parent ID）、可见性模型统一（per-device flag）、Topology/Pci 职责划分、SystemSnapshot 元数据、RawStore 多记录、AcceleratorSubsystem 定义、Pipeline 中间表示（ParsedFacts）、冲突解决策略（source trust order）、序列化、线程安全、缓存预留、跨平台扩展、测试策略（raw replay）。每项含问题陈述、C++ 类型定义、理由、对原设计影响。未修改原有 design_document.md。
- **原因**: design_document.md 初版存在命名不自洽、层级关系丢失、冲突策略缺失等问题，需在实现前补齐设计
- **验证**: 文档审阅（无代码变更）

### 2026-06-20 合并设计提案为唯一设计文档

- **变更类型**: docs
- **涉及文件**: docs/design_document.md, docs/design_proposals.md (删除), README.md, docs/devlog.md
- **变更内容**: 将 14 项设计提案合并进 design_document.md，形成唯一完整设计文档（23 节）。补齐了原文档缺失的类型定义（CollectStatus、NetworkSubsystem、StorageSubsystem、NumaNode、NumaMemoryInfo、Architecture、IsaExtension、StrongId、Expected 等）。修正了 API 风格不一致（README 用方法调用 `snapshot.resources()` 改为成员访问 `snapshot.resources`）。新增章节：冲突解决策略（§12）、线程安全（§16）、序列化（§17）、测试策略 raw replay（§18）、未来扩展（§19）。删除 docs/design_proposals.md。
- **原因**: 消除多份文档间的混乱，确保初开发阶段只有一份自洽的完整设计文档
- **验证**: 文档审阅，无代码变更

### 2026-06-20 拆分设计文档为按层级组织的小文件

- **变更类型**: docs / refactor
- **涉及文件**: docs/design_document.md (删除), docs/design/index.md, docs/design/overview.md, docs/design/public_api.md, docs/design/data_model/*.md (8 files), docs/design/architecture/*.md (2 files), docs/design/rules/*.md (3 files), docs/design/testing/*.md (2 files), docs/design/roadmap.md, docs/devlog.md
- **变更内容**: 将单文件 design_document.md (23 节, 1144 行) 拆分为 19 个独立小文件，按架构层级组织在 docs/design/ 下：概览层、公共 API 层、数据模型层 (8 文件)、内部架构层 (2 文件)、设计规则层 (3 文件)、测试层 (2 文件)、路线图层。创建 index.md 作为索引目录，含目录结构、文档索引表、推荐阅读顺序。删除原 design_document.md。
- **原因**: 单文件过大难以导航和维护，拆分后每个文件职能单一、便于独立查阅和更新
- **验证**: 文档审阅，无代码变更

### 2026-06-20 同步 base_project CI workflow 更新

- **变更类型**: ci
- **涉及文件**: .github/workflows/ci.yml, docs/devlog.md
- **变更内容**: CI workflow 更新：ubuntu-latest → ubuntu-24.04；安装包加 build-essential / libc++abi-dev，去 libunwind-dev；xmake 安装改用 GitHub Action xmake-io/github-action-setup-xmake@v1；新增 Configure (xmake f -c -y --toolchain=clang) 和 Build (xmake build) 步骤
- **原因**: 与 base_project 模板保持同步
- **验证**: 文件比对一致

### 2026-06-21 实现 v0.0.1 内部管线与公共 API

- **变更类型**: src / build / fix
- **涉及文件**: xmake.lua, .clang-tidy, include/sysal/collect.hpp, src/sysal.cpp, src/parser/parsed_facts.hpp, src/parser/parse_utils.hpp, src/parser/cpu_parser.hpp, src/parser/cpu_parser.cpp, src/parser/memory_parser.hpp, src/parser/memory_parser.cpp, src/parser/platform_parser.hpp, src/parser/platform_parser.cpp, src/parser/network_parser.hpp, src/parser/network_parser.cpp, src/parser/pci_parser.hpp, src/parser/pci_parser.cpp, src/reader/linux/file_utils.hpp, src/reader/linux/procfs_reader.hpp, src/reader/linux/procfs_reader.cpp, src/reader/linux/sysfs_reader.hpp, src/reader/linux/sysfs_reader.cpp, src/resolver/resolver.hpp, src/resolver/resolver.cpp, src/public_api/collect.cpp, tests/test_collect.cpp, docs/devlog.md
- **变更内容**: 实现完整 Reader→RawStore→Parser→ParsedFacts→Resolver→SystemSnapshot 管线。新增公共 API 声明头 include/sysal/collect.hpp（collect / collect_or_throw）。新增内部 ParsedFacts 结构（CpuFacts / MemoryFacts / PciFacts / NetworkFacts / PlatformFacts）。新增 Linux procfs reader（/proc/cpuinfo、/proc/meminfo、/proc/version、/etc/os-release、uname syscall）和 sysfs reader（CPU 拓扑、NUMA、网络接口、PCI 设备）。新增 5 个 parser（CPU/内存/平台/网络/PCI），将原始数据解析为 ParsedFacts。新增 resolver 将 ParsedFacts 组装为 SystemSnapshot，填充 SnapshotMeta、ExecutionContextInfo、可见性标志。新增 collect.cpp 实现 collect() 和 collect_or_throw() 公共 API。xmake.lua 添加 add_includedirs("src") 和 test_collect 二进制目标。.clang-tidy 添加 -bugprone-unchecked-optional-access 抑制（expected.hpp 中 operator*/error 为有意设计，且该头文件不可修改）。
- **原因**: sysal v0.0.1 核心功能实现，需要可工作的 collect() / collect_or_throw() 从 Linux 系统收集 CPU、内存、平台、网络信息
- **验证**: `xmake -r` 构建成功；`clang-format --dry-run --Werror` 全部通过；`clang-tidy` 零警告；`xmake run test_collect` 输出合理的 CPU 包数、逻辑 CPU 数、内存总量、主机名、OS 版本、内核版本信息

### 2026-06-21 架构简化 — 消除 ParsedFacts 类型重复 + collect_spec inline + resolver move

- **变更类型**: refactor / src
- **涉及文件**: src/parser/parsed_facts.hpp, src/parser/cpu_parser.hpp, src/parser/cpu_parser.cpp, src/parser/memory_parser.hpp, src/parser/memory_parser.cpp, src/parser/platform_parser.hpp, src/parser/platform_parser.cpp, src/parser/network_parser.hpp, src/parser/network_parser.cpp, src/parser/pci_parser.hpp, src/parser/pci_parser.cpp, src/resolver/resolver.hpp, src/resolver/resolver.cpp, src/public_api/collect.cpp, include/sysal/collect_spec.hpp, src/collect_spec.cpp (删除), docs/devlog.md
- **变更内容**:
  1. `parsed_facts.hpp` 删除 CpuFacts/MemoryFacts/PciFacts/NetworkFacts/PlatformFacts 5 个重复类型定义，改用公共类型 CpuSubsystem/MemorySubsystem/PciSubsystem/NetworkSubsystem/PlatformInfo
  2. 5 个 parser 头文件和实现文件的返回类型同步更新
  3. `resolver.cpp` 删除 5 个 fill_* 逐字段拷贝函数，改为 `std::move(*facts.xxx)` 直接赋值；resolve 签名改为右值引用 `ParsedFacts&&`
  4. `collect_spec.hpp` 全部 22 个 getter/setter + 3 个工厂方法 inline 到头文件；删除 `src/collect_spec.cpp`（135 行）
  5. `collect.cpp` 调用 resolve 时传 `std::move(facts)`
- **原因**: ParsedFacts 重复定义了与公共类型 1:1 映射的内部类型，resolver 逐字段拷贝是纯冗余；collect_spec 22 个 boilerplate 函数不应有 .cpp 文件
- **验证**: `xmake -r` 构建成功；`clang-format` 通过；`xmake run test_collect` 输出不变

### 2026-06-21 消除代码冗余 — 提取共享抽象

- **变更类型**: refactor / src
- **涉及文件**: src/detail/algorithm.hpp, src/resource_info.cpp, src/reader/linux/procfs_reader.cpp, src/reader/linux/sysfs_reader.cpp, src/parser/parse_utils.hpp, src/parser/cpu_parser.cpp, src/parser/platform_parser.cpp, src/parser/network_parser.cpp, src/parser/pci_parser.cpp, src/raw_store.cpp, include/sysal/value_types.hpp, include/sysal/units.hpp, include/sysal/diagnostics.hpp, tests/test_collect.cpp, docs/devlog.md
- **变更内容**:
  1. 新增 `src/detail/algorithm.hpp`，提供 `find_if` / `filter_if` 模板；`resource_info.cpp` 12 个线性搜索方法全部改用模板
  2. `procfs_reader.cpp` 提取 `read_proc_file` 辅助函数，删除 4 个近重复函数
  3. `sysfs_reader.cpp` 提取 `read_sysfs_dir` 目录遍历辅助函数，4 个 sysfs 读取函数统一改用该辅助
  4. `parse_utils.hpp` 新增 `arch_from_machine` 和 `extract_prefix_keys` 共享工具；`cpu_parser.cpp` 删除本地 `determine_arch` 中的重复映射逻辑，`platform_parser.cpp` 删除本地 `arch_from_machine`；`network_parser.cpp` 和 `pci_parser.cpp` 删除各自的 `extract_interface_names` / `extract_device_addresses`，统一调用 `extract_prefix_keys`
  5. `value_types.hpp` 用 `NamedString<Tag>` 模板替换 5 个重复字符串包装结构体
  6. `units.hpp` 用 `ScalarUnit<Tag>` 模板替换 3 个重复标量单位结构体；`test_collect.cpp` 中 `.bytes` 访问改为 `.value`
  7. `raw_store.cpp` 的 `count` 方法改用 `std::ranges::count_if`
  8. `diagnostics.hpp` 移除不必要的 `#include "sysal/raw_store.hpp"`，改为直接包含 `<vector>`
- **原因**: 消除 12+4+4+2+5+3 个近重复代码模式，提升可维护性；统一抽象后新增类型只需复用模板
- **验证**: `utils/check.sh` 全部 4 项检查通过（clang-format / clang-tidy / build / tests）；`xmake run test_collect` 输出与重构前一致

### 2026-06-21 实现 execution_parser 执行上下文解析器

- **变更类型**: src
- **涉及文件**: src/parser/execution_parser.cpp, docs/devlog.md
- **变更内容**:
  1. 进程信息：`getpid/getuid/getgid/geteuid/getegid` 填充 `ProcessInfo`，`is_root = (geteuid == 0)`
  2. Cgroup 解析：从 `RawSource::ProcSelfCgroup` 读取，`0::` 前缀检测 v2，否则按 v1 取首行路径
  3. Cpuset 解析：从 `RawSource::ProcSelfStatus` 读取 `Cpus_allowed_list` / `Mems_allowed_list`，`parse_id_list` 支持 `N` 和 `N-M` 范围语法，填充 `LogicalCpuId` / `NumaNodeId`；cpus 非空时 `is_restricted = true`
  4. 容器检测：`/.dockerenv` 存在 → Docker；`/proc/1/cgroup` 内容匹配 docker/podman/lxc/kube 模式；环境变量 `container` → Podman、`KUBERNETES_SERVICE_HOST` → Kubernetes；无匹配则 `container = nullopt`
  5. 环境变量：`getenv` 读取 `CUDA_VISIBLE_DEVICES` / `HIP_VISIBLE_DEVICES` / `ONEAPI_DEVICE_SELECTOR` / `OMP_NUM_THREADS` / `MLU_VISIBLE_DEVICES`，仅收录已设置变量
  6. 权限能力：从 `ProcSelfStatus` 读取 `CapEff` 十六进制串存入 `PermissionInfo::capabilities`
- **原因**: 完成 v0.0.1 执行上下文收集功能，覆盖进程/cgroup/cpuset/容器/环境/权限六个维度
- **验证**: `xmake -r` 构建通过（-Wall -Wextra -Werror 零 warning）；LSP diagnostics 无报错

### 2026-06-21 实现 accelerator_parser 和 software_parser

- **变更类型**: src
- **涉及文件**: src/parser/accelerator_parser.cpp, src/parser/software_parser.cpp, docs/devlog.md
- **变更内容**:
  1. `accelerator_parser.cpp`：解析 `RawSource::NvidiaSmi` 中 `path_or_command` 以 "nvidia-smi" 开头的成功记录，按 CSV 格式（index, name, memory.total, pci.bus_id, driver_version）逐行解析。每行 split by comma + trim，构造 `AcceleratorDevice`（kind=Gpu, vendor=NVIDIA, memory 由 MiB 转 bytes, pci_address 用 `parse_pci_address`，visible=true）。无数据或命令失败返回 `nullopt`
  2. `software_parser.cpp`：从三处来源提取 NVIDIA 软件栈信息：(a) `/proc/driver/nvidia/version` 的 NVRM version 行提取驱动版本（空格分隔的 version-like token）；(b) `nvcc --version` 的 release 行提取 CUDA runtime 版本；(c) nvidia-smi CSV 末列提取驱动版本并统计 device_count。填充 `CudaInfo`、`DriverInfo{name=nvidia, loaded=true}`、`RuntimeInfo{name=cuda}`。无 NVIDIA 软件返回 `nullopt`
- **原因**: 完成 v0.0.1 GPU 加速器和软件栈解析功能
- **验证**: `xmake -r` 构建通过；`clang-format --dry-run --Werror` 两文件通过；`clang-tidy` 零警告；LSP diagnostics 无报错

### 2026-06-21 扩展 network_parser 并实现 storage_parser

- **变更类型**: src
- **涉及文件**: src/parser/network_parser.cpp, src/parser/storage_parser.cpp, docs/devlog.md
- **变更内容**:
  1. `network_parser.cpp`：在原有 name/state/speed/MAC 解析基础上新增两项。(a) IP 地址：通过 `getifaddrs()` 系统调用直接获取每个接口的 AF_INET/AF_INET6 地址，用 `inet_ntop` 转为字符串存入 `NetworkInterface::addresses`，避免解析复杂的 `/proc/net/fib_trie`。(b) PCI 地址：从 `path_map` 查找 `/sys/class/net/<iface>/device` 记录（sysfs_reader 已读取 device 符号链接的 filename），用 `parse_pci_address()` 解析后存入 `NetworkInterface::pci_address`。`diag` 参数从忽略改为使用，接口列表为空时发 warning
  2. `storage_parser.cpp`：从 stub 改为完整实现。用 `build_path_map` + `extract_prefix_keys`（前缀 `/sys/block/`）提取块设备名列表。对每个设备：从 `size` 记录解析扇区数 × 512 得 `MemorySize` capacity；从 `device/model` 解析型号覆盖 DeviceName；从 `device` 符号链接解析 PCI 地址；按名称前缀分类（nvme→Nvme, sd→Sata, else→Other）；构造 `StorageDevice{StorageId{index}, ...}`。无数据返回 `nullopt`
- **原因**: 完成 v0.0.1 网络接口 IP/PCI 地址收集和块设备存储解析功能
- **验证**: `xmake -r` 构建通过（-Wall -Wextra -Werror 零 warning）；`clang-format -i` 两文件通过；`clang-tidy` 零警告；LSP diagnostics 无报错

### 2026-06-21 实现 topology 解析、NUMA 内存解析与 hwloc 后端

- **变更类型**: src
- **涉及文件**: src/parser/topology_parser.cpp, src/parser/memory_parser.cpp, src/backend/hwloc_backend.hpp, src/backend/hwloc_backend.cpp, docs/devlog.md
- **变更内容**:
  1. `topology_parser.cpp`：从 stub 改为完整实现。优先尝试 hwloc 后端（`parse_topology_hwloc`），成功则直接返回；否则回退到 sysfs 解析。sysfs 路径用 `build_path_map` + `extract_prefix_keys`（前缀 `/sys/devices/system/node/`）提取 NUMA 节点列表，从 `node<N>/meminfo` 解析 `MemTotal` 行（KB→bytes）构造 `NumaRelation{node_id, packages=空, local_memory}`；用前缀 `/sys/bus/pci/devices/` 提取 PCI 设备列表，从 `numa_node` 文件解析节点号（跳过负值 -1），构造 `DeviceLocality{pci_address, nearest_numa_node}`。无数据返回 `nullopt`
  2. `memory_parser.cpp`：在 `/proc/meminfo` 解析基础上新增 NUMA 内存解析。用 `build_path_map` + `extract_prefix_keys`（前缀 `/sys/devices/system/node/`）遍历各 NUMA 节点，从 `node<N>/meminfo` 解析 `MemTotal`（→total）和 `MemFree`（→available），构造 `NumaMemoryInfo` 填入 `MemorySubsystem::numa_memory` 向量
  3. `hwloc_backend.hpp`：声明 `parse_topology_hwloc(Diagnostics&) -> optional<TopologyInfo>`
  4. `hwloc_backend.cpp`：用 `#ifdef SYSAL_HAVE_HWLOC` 条件编译包裹 hwloc API 调用。启用 `HWLOC_OBJ_PCI_DEVICE` 类型过滤器后 `hwloc_topology_load`；遍历 `HWLOC_OBJ_NUMANODE` 层读取 `obj->attr->numanode.local_memory` 构造 `NumaRelation`；遍历 `HWLOC_OBJ_PCI_DEVICE` 层读取 `obj->attr->pcidev.{domain,bus,dev,func}` 构造 `PciAddress`，沿 parent 链向上查找 memory_first_child 中的 NUMA 节点作为 `nearest_numa_node` 构造 `DeviceLocality`。未定义 `SYSAL_HAVE_HWLOC` 时直接返回 `nullopt`
- **原因**: 完成 v0.0.1 拓扑信息收集（NUMA 关系、PCI 设备局部性）和 NUMA 内存分布解析，支持 hwloc 可选后端
- **验证**: `xmake -r` 构建通过（-Wall -Wextra -Werror 零 warning）；`clang-format --dry-run --Werror` 四文件通过；`clang-tidy --warnings-as-errors='*'` 零用户代码警告

### 2026-06-21 实现 JSON 序列化与 raw replay 测试基础设施

- **变更类型**: src / build / docs
- **涉及文件**: include/sysal/serialization.hpp, src/serialization.cpp, include/sysal/test/replay.hpp, src/test/replay.cpp, src/detail/json.hpp, tests/test_replay.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. `include/sysal/serialization.hpp`：声明 `SerializationOptions`（pretty_print / include_raw / include_meta）和公共 API `to_json` / `from_json`
  2. `src/detail/json.hpp`：内部 JSON 工具库（header-only）。包含 `escape_string`、`JsonObj` / `JsonArr` 构建器、`JsonVal` DOM 节点、`JsonParser` 递归下降解析器、`parse_json`、`raw_store_to_json` / `raw_store_from_json`、`time_point_to_ms` / `ms_to_time_point`。手写实现，无外部依赖
  3. `src/serialization.cpp`：`to_json` 序列化 SystemSnapshot 全部字段（meta / platform / resources / software / execution / diagnostics / raw），枚举序列化为整数、StrongId 序列化为 `.value()`、NamedString 序列化为 `.value` 字符串、ScalarUnit 序列化为 `.value` 整数、optional 字段省略、time_point 序列化为 epoch 毫秒。`from_json` 解析 JSON DOM 并提取 meta 和 raw（v0.0.1 基本反序列化）
  4. `include/sysal/test/replay.hpp`：声明 `load_raw_store` / `collect_from_raw` / `save_raw_store`
  5. `src/test/replay.cpp`：`save_raw_store` 将 RawStore 序列化为 JSON 文件；`load_raw_store` 解析 JSON 文件为 RawStore；`collect_from_raw` 调用 `sysal::detail::run_pipeline(raw, spec, now)`
  6. `tests/test_replay.cpp`：capture → save → load → replay → verify 流程，验证 CPU 数量、内存总量、NUMA 节点、网络接口、PCI 设备、存储设备、加速器数量一致
  7. `xmake.lua`：新增 `test_replay` 目标
- **原因**: 实现 design_document §17 序列化和 §18 raw replay 测试策略，为回归测试提供基础设施
- **验证**: `xmake -r` 构建通过；`xmake run test_replay` 全部 9 项检查 PASS；`utils/check.sh` 4/4 通过（clang-format + clang-tidy + build + tests）

### 2026-06-21 v0.0.1 全功能完成：基础设施 + pipeline 重构

- **变更类型**: src / build / refactor
- **涉及文件**: xmake.lua, include/sysal/enums.hpp, src/parser/parsed_facts.hpp, src/parser/{topology,execution,accelerator,software,storage}_parser.{hpp,cpp}, src/reader/linux/{procfs,sysfs}_reader.cpp, src/reader/linux/file_utils.hpp, src/resolver/resolver.cpp, src/public_api/collect.cpp, src/detail/pipeline.{hpp,cpp}, src/backend/hwloc_backend.{hpp,cpp}, tests/test_collect.cpp
- **变更内容**:
  1. 基础设施：enums 新增 SysfsBlock/ProcSelfCgroup/ProcSelfStatus/ProcNetInet；ParsedFacts 新增 accelerator/storage/topology/software/execution 字段；xmake.lua 添加 hwloc pkg-config 探测（SYSAL_HAVE_HWLOC）
  2. Reader 扩展：procfs_reader 新增 cgroup/status/fib_trie/nvidia-smi/nvcc 读取；sysfs_reader 新增 NUMA meminfo/PCI numa_node/network device symlink/block device 读取；file_utils 新增 read_command (popen)
  3. Pipeline 重构：提取 src/detail/pipeline.{hpp,cpp} 共享 parse+resolve 逻辑，collect.cpp 和 replay.cpp 共用
  4. Resolver 增强：处理 accelerator/storage/topology/software/execution；cpuset 驱动 CPU 可见性；预计算 visible_*_ids 索引
  5. test_collect 升级为 CollectSpec::full() 全功能验证
- **原因**: 完成 v0.0.1 roadmap 所有目标——CPU/Memory/NUMA/Accelerator/Network/PCI/Storage/Topology/Software/Execution/Serialization/RawReplay
- **验证**: `utils/check.sh` 4/4 通过；`xmake run test_collect` 输出 2 CPU packages, 52 logical CPUs, 405GB memory, 2x A100 80GB GPU, 304 PCI devices, 13 storage devices, 2 NUMA relations, 10 network interfaces; `xmake run test_replay` 9/9 PASS

### 2026-06-21 代码质量清理：去冗余 + Bug 修复 + 错误处理补全

- **变更类型**: refactor / fix / src
- **涉及文件**: src/parser/parse_utils.hpp, src/parser/memory_parser.cpp, src/parser/topology_parser.cpp, src/resolver/resolver.cpp, src/parser/execution_parser.cpp, src/parser/network_parser.cpp, src/parser/platform_parser.cpp, src/parser/pci_parser.cpp, src/parser/storage_parser.cpp, src/parser/software_parser.cpp, src/reader/linux/procfs_reader.cpp, src/sysal.cpp(删除)
- **变更内容**:
  1. **去冗余**：`extract_kb`/`node_id_from_key`/`kNodePrefix` 从 memory_parser + topology_parser 合并到 parse_utils.hpp；删除 src/sysal.cpp 空文件；删除 procfs_reader 中未使用的 /proc/net/fib_trie 读取
  2. **Bug 修复**：删除 resolver 中 `fill_execution_context` 死代码（与 execution_parser 重复，覆盖已填充的值）；修复 `is_restricted` 启发式（原逻辑：cpus 非空即 restricted → 所有系统都被标记为 restricted；新逻辑：cpuset 是 CPU 总数的真子集时才 restricted）；修复 resolver 可见性 O(n×m) → O(n+m) 使用 `unordered_set<LogicalCpuId>` 查找；修复 network_parser O(n²) getifaddrs（原：每个接口单独调用 getifaddrs 遍历全部；新：调用一次构建 name→addresses 映射）
  3. **错误处理补全**：5 个忽略 `diag` 参数的 parser 改为使用 `add_warning`：platform_parser（缺少 uname 架构信息时告警）、pci_parser（sysfs 记录存在但无设备时告警）、storage_parser（同上）、software_parser（NVIDIA 数据收集失败时告警）、execution_parser（无法确定 cgroup 路径时告警）
- **原因**: 代码质量审计发现 7 项冗余 + 4 项 Bug + 5 个 parser 缺失错误处理
- **验证**: `utils/check.sh` 4/4 通过；`xmake run test_replay` 9/9 PASS

### 2026-06-27 为 parser 第一组文件添加中文 Doxygen 注释

- **变更类型**: docs
- **涉及文件**: src/parser/cpu_parser.hpp, src/parser/cpu_parser.cpp, src/parser/memory_parser.hpp, src/parser/memory_parser.cpp, src/parser/network_parser.hpp, src/parser/network_parser.cpp, src/parser/platform_parser.hpp, src/parser/platform_parser.cpp, src/parser/pci_parser.hpp, src/parser/pci_parser.cpp
- **变更内容**:
  1. 每个文件开头添加文件级 Doxygen 注释块（@file/@brief/@details）
  2. 每个函数（含匿名命名空间内函数）添加 Doxygen 头注释（@brief/@param/@return）
  3. 匿名命名空间内结构体（CpuInfoEntry）添加注释说明
  4. 函数内部关键逻辑添加行内中文注释：CPU 拓扑去重与回退策略、NUMA meminfo 键前缀匹配、网络接口速率单位转换、/proc/version 字段位置解析等
  5. 代码逻辑、缩进、换行、#include、命名空间结构均未改动
- **原因**: 为解析器层补充中文 API 文档，便于团队理解解析逻辑
- **验证**: `utils/check.sh` 4/4 通过（clang-format / clang-tidy / build / tests）；10 个文件均通过 clang-format 校验

### 2026-06-27 为 reader/linux 与 detail 目录添加中文 Doxygen 注释

- **变更类型**: docs
- **涉及文件**: src/reader/linux/file_utils.hpp, src/reader/linux/procfs_reader.hpp, src/reader/linux/procfs_reader.cpp, src/reader/linux/sysfs_reader.hpp, src/reader/linux/sysfs_reader.cpp, src/detail/pipeline.hpp, src/detail/pipeline.cpp, src/detail/algorithm.hpp, src/detail/json.hpp
- **变更内容**:
  1. 9 个文件均添加文件级 `@file`/`@brief`/`@details` 注释块
  2. 每个函数（含匿名命名空间内函数、模板函数、类成员函数）添加 `@brief`/`@param`/`@return` Doxygen 注释
  3. 函数内部关键逻辑添加行内中文注释：procfs 各采集分支用途、sysfs 目录遍历与属性读取、pipeline 解析-组装流程、JSON 序列化/反序列化数据格式与转义/UTF-8 编码逻辑
  4. json.hpp 说明 RawStore JSON 格式（records 数组、source/status 整数编码、collected_at epoch 毫秒）
  5. 代码逻辑、#include、命名空间结构均未改动；仅添加注释，未删除已有注释
- **原因**: 为采集层与基础设施层补充中文 API 文档，与解析器层注释风格保持一致
- **验证**: `utils/check.sh` 4/4 通过（clang-format / clang-tidy / build / tests）

### 2026-06-27 为 include/sysal 公共头文件添加中文 Doxygen 注释

- **变更类型**: docs
- **涉及文件**: include/sysal/ 下全部 21 个 .hpp 文件（enums.hpp, error.hpp, expected.hpp, units.hpp, strong_id.hpp, value_types.hpp, ids.hpp, platform_info.hpp, raw_store.hpp, diagnostics.hpp, collect_spec.hpp, collect.hpp, resource_info.hpp, topology_info.hpp, snapshot_meta.hpp, software_stack_info.hpp, execution_context_info.hpp, serialization.hpp, system_snapshot.hpp, sysal.hpp, test/replay.hpp）
- **变更内容**:
  1. 每个文件开头添加文件级 `@file`/`@brief`/`@details` 注释块
  2. 每个结构体/类添加 `@brief`/`@details` Doxygen 注释
  3. 每个结构体关键成员变量添加 `///<` 行尾注释
  4. 每个函数声明/方法添加 `@brief`/`@param`/`@return`/`@throws` 注释（简单 getter/setter 仅 @brief）
  5. 所有枚举类型与枚举值添加注释；类型别名（using）添加行尾注释
  6. 保留 strong_id.hpp 已有英文 Doxygen 注释并补充其余注释
  7. 代码逻辑、缩进、#include、#pragma once、命名空间结构均未改动
- **原因**: 为公共 API 头文件补充完整中文 Doxygen 文档，提升 API 可发现性与可维护性
- **验证**: `utils/check.sh` 4/4 通过（clang-format / clang-tidy / build / tests）

### 2026-06-27 为核心文件、解析器、后端、公共 API、测试添加中文 Doxygen 注释

- **变更类型**: docs
- **涉及文件**: src/raw_store.cpp, src/serialization.cpp, src/resource_info.cpp, src/resolver/resolver.hpp, src/resolver/resolver.cpp, src/backend/hwloc_backend.hpp, src/backend/hwloc_backend.cpp, src/public_api/collect.cpp, src/test/replay.cpp, docs/devlog.md
- **变更内容**:
  1. 为 9 个核心文件添加文件级 Doxygen 注释块（@file/@brief/@details）
  2. 为每个函数（含匿名命名空间内函数与条件编译分支内的函数）添加 Doxygen 头注释（@brief/@param/@return/@throws/@details）
  3. 函数内部关键逻辑添加行内中文注释：
     - resolver.cpp 说明从 ParsedFacts 构建 SystemSnapshot 的流程（子字段移动、默认可见性、cpuset 覆盖、可见资源汇总）
     - serialization.cpp 每个 to_json_* 函数说明 JSON 输出格式（对象键名、数组结构、可选字段、枚举/数值/布尔/PCI 地址的表示方式）
     - hwloc_backend.cpp 说明 NUMA 节点查找、PCI 设备遍历与拓扑加载流程
   4. 代码逻辑、#include、#pragma once、命名空间结构均未改动；未删除已有注释（将 resolver.cpp 中原有英文行内注释译为中文以统一注释语言）
- **原因**: 为库的核心实现层补充中文 API 文档，与已有头文件/解析器层 Doxygen 风格保持一致，便于维护与生成文档
- **验证**: `utils/check.sh` 4/4 通过（clang-format / clang-tidy / build / tests）

### 2026-06-27 数据模型设计文档中文化

- **变更类型**: docs
- **涉及文件**: docs/design/data_model/software_stack_info.md, docs/design/data_model/execution_context.md, docs/design/data_model/raw_store.md, docs/design/data_model/diagnostics.md
- **变更内容**: 将 4 个数据模型设计文档从英文重写为中文。翻译正文、表格内容、代码块内注释；保持代码逻辑、字符串字面量、类型名/函数名/变量名、Markdown 结构不变；保留 examples 示例块（英文输出文本）原样以体现真实采集结果；专有名词（CUDA、ROCm、MPI、RDMA、UCX、NVML、Level Zero 等）保持英文。
- **原因**: 统一设计文档语言为中文，与项目其余文档及代码注释风格一致
- **验证**: 逐文件对照原文确认结构、代码块、表格、列表完整保留

### 2026-06-30 重写测试相关设计文档以反映架构变更

- **变更类型**: docs
- **涉及文件**: docs/design/testing/serialization.md, docs/design/testing/raw_replay.md
- **变更内容**: 重写 2 个测试相关设计文档为中文，反映新架构：`System` 类替代 `collect()`/`collect_or_throw()`，`Collect` 位掩码枚举替代 `CollectSpec`，移除 `Expected<T,E>`，所有接口失败时抛 `SysalError`。serialization.md 接口改为 `to_json(const System&, ...)` / `from_json(std::string_view)`，强调非侵入式自由函数、独立头文件、手写 JSON 序列化、`SnapshotMeta::sysal_version` 兼容性检查。raw_replay.md 接口改为 `load_raw_store` / `collect_from_raw` / `save_raw_store` 直接返回值；工作流使用 `System::collect` + `Collect::Raw`；管线对比更新为 `Reader → RawStore → Parser → ParseResult → Resolver → System`；保留 Fixture 布局。
- **原因**: sysal 公共 API 架构变更（异常替代 Expected、System/Collect 替代旧 API），测试相关设计文档需同步
- **验证**: 逐文件确认无 Expected/CollectSpec/collect_or_throw 残留，C++ 代码块语法正确，专有名词与类型名保持英文

### 2026-06-30 重写 4 个核心设计文档反映架构变更

- **变更类型**: docs
- **涉及文件**: docs/design/index.md, docs/design/overview.md, docs/design/public_api.md, docs/design/roadmap.md
- **变更内容**: 用 Write 覆盖重写 4 个核心设计文档以反映新架构：
  1. index.md：目录结构移除 topology_info.md / diagnostics.md（diagnostics 改为 warnings 内嵌 System），文档索引表移除 topology_info 行、更新各文档描述（overview 移除拓扑、public_api 改为 System 类、pipeline 改为新文件命名 ParseResult、backend_strategy 移除 hwloc）、阅读顺序更新
  2. overview.md：管线图改为 `Reader → RawStore → Parser → ParseResult → Resolver → System`，架构总结移除拓扑构建、改为 System 对象，API 调用示例改为 `System::collect()`
  3. public_api.md：完整重写，描述 System 类（对象持有模式）、Collect 位掩码枚举、operator| 与 has()、basic/full 预设、System 完整接口、使用示例（链式 flags + 预设）、失败抛 SysalError 而非 Expected、公共 API 不暴露内部 reader/parser/backend
  4. roadmap.md：v0.0.1 范围改为 System::collect/refresh + Collect bitmask、Core model 移除 Topology 与 Diagnostics（改为 warnings）、内部管线 ParseResult、移除 hwloc 后端、非目标添加"拓扑信息（已有 hwloc 等成熟库）"、未来扩展说明缓存已内置（System 对象即缓存）、拓扑作为独立可选模块
- **原因**: API 重构（移除 collect/collect_or_throw 双入口、Expected、CollectSpec builder，引入 System 类 + Collect bitmask）、移除 hwloc 拓扑后端、ParsedFacts 重命名为 ParseResult、Diagnostics 简化为 warnings，设计文档需同步反映
- **验证**: 逐文件确认无 CollectSpec / collect() / collect_or_throw() / Expected / TopologyInfo / hwloc 残留描述；代码块 C++ 语法正确；专有名词与类型名保持英文
