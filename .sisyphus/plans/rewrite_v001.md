# sysal v0.0.1 重写计划

## 目标

按照 `docs/design/` 下 23 个冻结的设计文档，从零重写 sysal C++23 系统信息库的全部代码。

## 约束

- C++23 / xmake / clang / libc++ / lld / compiler-rt
- `-Wall -Wextra -Werror`，clang-tidy `WarningsAsErrors: '*'`
- 每个阶段必须 `utils/check.sh` 全绿（format + tidy + build + tests）
- 每个阶段 = 1 个原子 commit，每个阶段必须可独立构建
- 注释使用中文
- 不动设计文档（已冻结）
- 命名规则：`docs/naming_rules.md`（namespace `snake_case`，class/struct `PascalCase`，function/variable `snake_case`，enum class `PascalCase`，file `snake_case`）

## 复用文件（4 个，移到 `include/sysal/types/`）

| 文件 | 行数 | 改动 |
|------|------|------|
| `strong_id.hpp` | 66 | 不改 |
| `ids.hpp` | 57 | 不改 |
| `units.hpp` | 43 | 不改 |
| `value_types.hpp` | 68 | 加 `PciClassTag` + `using PciClass = NamedString<PciClassTag>;` |

## 删除文件

除上述 4 个文件外，`include/sysal/`、`src/`、`tests/` 下的全部 60 个文件。

## 需要确认的 7 个决策（已采纳默认值）

1. `RawSource` 枚举：以设计文档 11 个为基底，按实现需要扩展（去掉 `HwlocXml`，加 `ProcSelfStatus`/`ProcOneCgroup`/`RootDockerenv`/`Environment` 等 C-1 修复所需的值）
2. `IsaExtension`：v0.0.1 只保留设计文档的 8 个（Sse42…Avx512vl），ARM/AMX 延后
3. 补上 `src/parser/platform.hpp/.cpp`（pipeline.md 遗漏了 platform parser）
4. `ErrorKind`：删掉 `PartialCollection`（部分失败走 warnings，不抛异常）
5. NVML：v0.0.1 accelerator parser 只读 sysfs + `nvidia-smi` 命令输出，不链接 NVML 库
6. 删除时机：先删后建（P0 一次性清空）
7. replay 实现位置：`load/save_raw_store` 放 `src/serialization/serialize.cpp`；`collect_from_raw` 放 `src/pipeline/pipeline.cpp`

## 阶段总览

```
Wave 1: P0 (清空+脚手架)
Wave 2: P1 (基础类型) ║ P4 (JSON 引擎)          ← 并行
Wave 3: P2 (数据模型 + System 外壳)
Wave 4: P3 (ParseResult) ║ P5 (RawStore 序列化) ║ P6 (Linux Reader)  ← 并行
Wave 5: P7a (platform parser) → P7b~P7i (8 个 parser 并行)
Wave 6: P8 (Resolver + Pipeline + System::collect)
Wave 7: P9 (System 序列化)
Wave 8: P10 (Fixtures + 测试 + 收尾)

关键路径: P0→P1→P2→P3→P7→P8→P9→P10
```

## 依赖图

| 阶段 | 依赖 | 原因 |
|------|------|------|
| P0 | 无 | 起点，清空树 |
| P1 | P0 | 需要新树 + xmake |
| P2 | P1 | 模型类型引用 enums/IDs/units/Collect |
| P3 | P2 | ParseResult 字段是 model optionals |
| P4 | P0 | 纯 JSON，无模型依赖 |
| P5 | P2, P4 | 需要 RawStore 模型 + json 引擎 |
| P6 | P1, P2 | 产出 RawStore（RawSource + RawStore） |
| P7 | P3 | 消费 RawStore，产出 ParseResult optionals |
| P8 | P6, P7, P2 | 编排 reader→parser→resolver→System |
| P9 | P8, P4 | 序列化组装好的 System |
| P10 | P8, P9 | fixture 需要 collect + save；测试需要序列化 |

---

## Phase 0 — 清空与脚手架 (S)

**做什么：** 删除全部 60 个旧文件；将 4 个保留文件移到 `include/sysal/types/`；精简 xmake.lua 为可构建的空库；创建新目录结构；补上 check.sh 的 tests 执行逻辑；删 CI 中的 hwloc 依赖。

**文件：**
- 删除：`include/sysal/` 下除 4 个保留文件外的所有文件；`src/` 全部；`tests/` 全部
- 移动：`strong_id.hpp`、`ids.hpp`、`units.hpp`、`value_types.hpp` → `include/sysal/types/`
- 编辑 `xmake.lua`：删除 `on_load` 中的 hwloc 检测块；保留 `sysal` target（`src/**.cpp` glob → 空，构建空静态库）；删除 3 个 `test_target` 行（后续随测试逐步加回）；保留 `after_build` compile_commands 生成器
- 编辑 `utils/check.sh`：补上 tests 段的实际执行逻辑（遍历 `xmake run test_*` 或执行所有测试 binary）
- 编辑 `.github/workflows/ci.yml`：删除 `libhwloc-dev` 安装行
- 创建目录：`include/sysal/{core,model,serialization,test}`，`src/{api,model,reader/linux,parser,resolver,serialization,pipeline}`

**成功标准：** `xmake -r` 构建空 `libsysal.a` 成功；`compile_commands.json` 重新生成；`utils/check.sh` 全绿（含 tests 段，此时无测试应直接 pass）

**Commit：** `chore: clean slate for v0.0.1 rewrite`

---

## Phase 1 — 基础类型 (S)

**做什么：** 全部 header-only 基础设施：枚举、移入的保留文件（含 PciClass 修复）、Collect 位掩码、SysalError。

**文件：**
- `include/sysal/types/enums.hpp` — `Arch`（从 `Architecture` 重命名）、`InterfaceState`、`StorageKind`、`AcceleratorKind`、`IsaExtension`（8 个值）、`RawSource`（设计文档 23 个值，已含 C-1 修复和数据空白修复所需的全部来源）、`CollectStatus`、`VirtualizationKind`、`CgroupVersion`、`ContainerKind`。**无 `Severity`。**
- `include/sysal/types/strong_id.hpp` — 移入，不改
- `include/sysal/types/ids.hpp` — 移入，不改
- `include/sysal/types/units.hpp` — 移入，不改
- `include/sysal/types/value_types.hpp` — 移入 + 加 `struct PciClassTag {};` 和 `using PciClass = NamedString<PciClassTag>;`
- `include/sysal/core/collect.hpp` — `enum class Collect : uint32_t`（9 域位 + `Raw`）、`constexpr operator|`、`constexpr bool has(Collect, Collect)`、`constexpr Collect basic`、`constexpr Collect full`
- `include/sysal/core/error.hpp` — `SysalError`（保留）、`ErrorKind` 去掉 `PartialCollection`
- 编辑 `.clang-tidy`：删除 `-bugprone-unchecked-optional-access` 抑制（`Expected` 已移除）

**成功标准：** `xmake build` 干净；`tests/test_types.cpp` 断言 `has(full, Collect::Cpu)`、`basic` 位值、`PciClass` 可构造、`Arch::X86_64` 存在。`utils/check.sh` 全绿。

**Commit：** `feat(types): foundation enums, strong IDs, units, value types, Collect bitmask, SysalError`

---

## Phase 2 — 数据模型 + System 外壳 (M)

**做什么：** 全部 11 个 model header、`System`/`SystemInfo`/`SnapshotMeta`、umbrella header、两个 model .cpp（查询方法 + RawStore 方法）。**必须逐字段对照设计文档中的 struct 定义，不可遗漏任何字段。**

**文件：**
- `include/sysal/model/snapshot_meta.hpp` — `SnapshotMeta`（`collect_time`: time_point, `sysal_version`: string, `collect_duration`: duration<double>, `requested_flags`: Collect, `succeeded_collectors`: vector<string>, `failed_collectors`: vector<string>）
- `include/sysal/model/platform.hpp` — `Platform`、`Host`（hostname, machine_id, product_name, vendor, serial）、`Os`（name, version, distribution, distribution_version, codename）、`Kernel`（release, version, compiled_at, architecture）、`Architecture`（name, bits, byte_order）、`Firmware`（bios_vendor, bios_version, bios_date, uefi）、`Virtualization`（kind, hypervisor, container）
- `include/sysal/model/cpu.hpp` — `CpuPackage`、`CpuCore`、`LogicalCpu`、`NumaNode`（id, cpus: vector<LogicalCpuId>）、`Cpu`（+ 7 个查询方法声明）
- `include/sysal/model/memory.hpp` — `NumaMemory`（node, total, optional<available>）、`Memory`（total_memory, optional<available_memory>, numa_memory）
- `include/sysal/model/accelerator.hpp` — `AcceleratorDevice`、`Accelerators`（+ 6 个查询方法声明）
- `include/sysal/model/network.hpp` — `NetworkInterface`（**无 rdma_device 字段**）、`Network`（+ 2 个查询方法声明）
- `include/sysal/model/storage.hpp` — `StorageDevice`、`Storage`
- `include/sysal/model/pci.hpp` — `PciDevice`（`device_class` 类型为 `PciClass` 不是 `std::string`）、`Pci`（+ find 声明）
- `include/sysal/model/software.hpp` — `SoftwareStack`、`Driver`（id: DriverId, name, version, loaded, path）、`Runtime`（name, version, path, env_var）、`Compiler`（name, version, path, target）、`Library`（name, version, path, kind）、`Cuda`（version, driver_version, nvcc_path, home）、`Rocm`、`LevelZero`、`Mpi`（implementation, version, path）、`RdmaStack`
- `include/sysal/model/execution.hpp` — `ExecutionContext`（+ 3 个可见性索引向量）、`Process`（pid, ppid, uid, gid, comm, exe, cwd）、`Environment`（entries: vector<pair<string,string>>）、`Cgroup`（version, path, controllers）、`Cpuset`（cpus, mems, cpus_effective, mems_effective）、`Permission`（euid, egid, capabilities, is_root）、`Container`（kind, id, runtime）
- `include/sysal/model/raw_store.hpp` — `RawRecord`、`RawStore`（方法声明）
- `include/sysal/core/system.hpp` — `System` 类 + `SystemInfo`
- `include/sysal/core/sysal.hpp` — umbrella include
- `src/model/raw_store.cpp` — `get_all`/`get`/`has`/`count`
- `src/model/resource.cpp` — 查询方法：`Cpu::*`、`Accelerators::*`、`Network::*`、`Pci::find`

**成功标准：** `xmake build` 链接库；`tests/test_model.cpp` 构造含 2 packages/cores/logical cpus 的 `Cpu`，断言 `find_package`、`logical_cpus_of_package`、`visible_logical_cpus` 正确；`Accelerators::gpus()`/`visible()` 过滤；`Pci::find`。`utils/check.sh` 全绿。

**Commit：** `feat(model): full data model structs, System shell, RawStore and query methods`

---

## Phase 3 — ParseResult + 解析工具 (S)

**做什么：** 内部 parser 契约和共享解析工具（从旧 `parse_utils.hpp` 改编）。

**文件：**
- `src/parser/parse_result.hpp` — `sysal::detail::ParseResult`，9 个 `std::optional<...>` 字段
- `src/parser/parse_utils.hpp` — `trim`、`split`、`parse_kv`、`parse_uint`、`parse_pci_address`（**必须用十六进制解析，修复 B-1 bug**）、`parse_memory_size`

**成功标准：** `tests/test_parse_utils.cpp` 覆盖 trim/split/parse_kv/parse_uint/parse_pci_address（含十六进制地址 `0000:0a:00.0`）边界。`utils/check.sh` 全绿。

**Commit：** `feat(parser): ParseResult contract and parse utilities`

---

## Phase 4 — 手写 JSON 引擎 (M)

**做什么：** 从旧 `src/detail/json.hpp`（825 行，81% 可复用）改编新的序列化引擎，去除 sysal 耦合。纯 JSON 值模型 + parser + emitter。

**文件：**
- `src/serialization/json.hpp` — `JsonValue`（null/bool/number/string/array/object）、`parse_json(string_view) -> JsonValue`、`dump_json(const JsonValue&, bool pretty) -> string`。从旧引擎提取纯 JSON 部分（`escape_string`/`JsonObj`/`JsonArr`/`JsonVal`/`JsonParser`/`parse_json`），删除 sysal 耦合部分（`raw_store_to_json`/`raw_store_from_json`），替换 `SysalError`/`Expected` 为独立错误类型

**成功标准：** `tests/test_json.cpp` 往返嵌套 object/array/string/转义字符；pretty-print 匹配预期。`utils/check.sh` 全绿。

**Commit：** `feat(serialization): hand-written JSON engine`

---

## Phase 5 — RawStore 序列化 + save/load (S)

**做什么：** 序列化 `RawStore` ↔ JSON（fixture 格式，不是完整 System），加上 `save_raw_store`/`load_raw_store`。

**文件：**
- `include/sysal/test/replay.hpp` — 声明 `load_raw_store`、`save_raw_store`（`collect_from_raw` 在 P8 添加）
- `src/serialization/serialize.cpp` — `raw_store_to_json`/`raw_store_from_json` + `save_raw_store`/`load_raw_store` 包装

**成功标准：** `tests/test_raw_store_io.cpp` 构造 `RawStore`，`save_raw_store` 到临时文件，`load_raw_store` 回来，断言 records 相等。`utils/check.sh` 全绿。

**Commit：** `feat(testing): RawStore JSON serialization and save/load_raw_store`

---

## Phase 6 — Linux Reader (M)

**做什么：** procfs/sysfs reader 填充 `RawStore`，从旧 reader 改编（路径知识可复用）。**必须修复 C-1 bug：将原来 parser 直接调用的 syscall 移到 Reader 层。**

**文件：**
- `src/reader/linux/file_utils.hpp` — `read_file`、`read_command`、`read_file_optional`、`read_symlink`
- `src/reader/linux/procfs.hpp` / `procfs.cpp` — 读取 `/proc/cpuinfo`、`/proc/meminfo`、`/proc/version`、`/etc/os-release`、`/proc/self/cgroup`、`/proc/self/status`（含 PID/UID/GID/EUID/EGID 行）、`/proc/1/cgroup`、`/.dockerenv`、环境变量（`CUDA_VISIBLE_DEVICES` 等）、`nvidia-smi`、`nvcc`、`uname -m`
- `src/reader/linux/sysfs.hpp` / `sysfs.cpp` — 遍历 `/sys/devices/system/{cpu,node}`、`/sys/bus/pci/devices`、`/sys/class/net`、`/sys/block`、`/sys/class/dmi/id`（固件信息）、`/sys/devices/system/cpu/cpuN/cpufreq`（CPU 频率，修复 D-1）

**成功标准：** `tests/test_reader.cpp`（仅 Linux，有守卫）读取 `/proc/cpuinfo` 断言 `raw.has(RawSource::ProcCpuInfo)` 且 `count > 0`；`file_utils` 用临时文件做单元测试；断言 `/.dockerenv` 和 `/proc/1/cgroup` 被采集到 RawStore。`utils/check.sh` 全绿。

**Commit：** `feat(reader): Linux procfs/sysfs readers into RawStore`

---

## Phase 7 — 域解析器 (L，内部可并行)

**做什么：** 10 个 parser，每个 `parse_xxx(const RawStore&, std::vector<std::string>& warnings) -> std::optional<T>`。parser 互相独立。**TDD 用代码内构造 RawStore**（不依赖 fixture）。

**子阶段：**
- **7a platform**（建立模式）— `src/parser/platform.{hpp,cpp}`。**纳入 D-6（固件/BIOS，读 `/sys/class/dmi/id/`）和 D-7（虚拟化检测，`systemd-detect-virt` 或 `/proc/1/environ`）**
- **7b cpu**（最难：`/proc/cpuinfo` + sysfs 拓扑，package→core→logical，ISA，numa_node）— `cpu.{hpp,cpp}`。**纳入 D-1（CPU 频率，从 sysfs cpufreq 读取）和 D-2（NUMA 归属，从 `/sys/devices/system/node/nodeN/cpulist` 反向映射）**
- **7c memory** — `memory.{hpp,cpp}`
- **7d pci** — `pci.{hpp,cpp}`。**`parse_pci_address` 必须用十六进制（修复 B-1）**
- **7e network** — `network.{hpp,cpp}`。**不调用 `getifaddrs`/`inet_ntop`，从 RawStore 读取**
- **7f accelerator**（sysfs PCI + `lspci`/`nvidia-smi` 命令输出；不链接 NVML 库）— `accelerator.{hpp,cpp}`。**纳入 D-4（accelerator NUMA 亲和，从 `/sys/bus/pci/devices/<addr>/numa_node` 读取）**
- **7g storage** — `storage.{hpp,cpp}`。**修复 B-2（NVMe 设备 PCI 地址追溯 symlink 链）**
- **7h software** — `software.{hpp,cpp}`。v0.0.1 只实现 NVIDIA 驱动 + CUDA runtime
- **7i execution**（cgroup/cpuset/容器检测）— `execution.{hpp,cpp}`。**不直接调用 syscall（修复 C-1），全部从 RawStore 读取**

**依赖：** P3（ParseResult + parse_utils）、P2（model 类型）。7a 先建立模式，7b~7i 并行。

**成功标准（每个子阶段）：** `tests/test_parse_<domain>.cpp` 代码内构造含手写 payload 的 `RawStore`，调用 `parse_<domain>`，断言结构化字段 + 错误输入时生成 warning。每个子阶段 = 独立 commit。`utils/check.sh` 全绿。

**Commits：** `feat(parser): platform` … `feat(parser): execution`

**风险（最高）：** 7b cpu parser（最复杂拓扑逻辑）和 7i execution（cgroup v1/v2 检测、cpuset 解析、容器检测全部从 RawStore 读取）。

---

## Phase 8 — Resolver + Pipeline + System::collect (L)

**做什么：** 合并 `ParseResult`，应用冲突解决（来源信任优先级），计算可见性，组装 `System`；接线 `System::collect`/`refresh` 和 `collect_from_raw`。

**文件：**
- `src/resolver/resolve.hpp` / `resolve.cpp` — `resolve(ParseResult&&, SnapshotMeta&, vector<string>& warnings) -> SystemInfo`。实现：
  - **冲突解决**：来源信任优先级（专用后端 > sysfs > procfs > 命令输出 > 推断），5 类规则（数量/可见性/标识/状态/归属），生成 `[conflict] <field>: <src1>=<val>, <src2>=<val>, adopted=<src>` 格式警告
  - **可见性计算**：cpuset → 逻辑 CPU ID 列表；设备 cgroup / `CUDA_VISIBLE_DEVICES` 等环境变量 → 加速器 ID 列表；网络命名空间 → 接口名列表。**不只是 CPU——accelerator 和 network 也必须计算可见性，不能默认 true**
  - **交叉检查**：各资源 `visible_to_current_process` vs `ExecutionContext::visible_*_ids` 不一致时记录 warning（设计文档明确要求"以各资源子域中的 visible_to_current_process 为准，并将差异记录到 sys.warnings"）
- `src/pipeline/pipeline.hpp` / `pipeline.cpp` — `run_pipeline(Collect flags) -> System`（reader→parser→resolver）；`run_replay(const RawStore&, Collect flags) -> System`（跳过 reader）。**为后端 init/shutdown 预留配对结构**（即使 v0.0.1 不链接 NVML，pipeline 也要有 init_backend()/shutdown_backend() 的调用点）
- `src/api/system.cpp` — `System::collect()` 调用 `run_pipeline`；`System::refresh()` 重新运行并替换成员
- `include/sysal/test/replay.hpp` — 添加 `collect_from_raw` 声明；实现在 `pipeline.cpp`

**依赖：** P6（reader）、P7（parser）、P2（System）

**成功标准：** `tests/test_resolve.cpp` 喂入合成 `ParseResult`，断言冲突 warning + 可见性索引；`tests/test_collect.cpp` 在开发机上运行 `System::collect()`，断言 `info.cpu.logical_cpus` 非空、`meta.succeeded_collectors` 有值；`collect_from_raw` 往返手建 `RawStore`。`utils/check.sh` 全绿。

**Commit：** `feat(pipeline): resolver, pipeline orchestration, System::collect/refresh, collect_from_raw`

**风险（最高）：** 可见性计算正确性（accelerator/network 不可默认 true）和冲突解决规则覆盖。完成后跑 `review-work`。

---

## Phase 9 — System 序列化 (M)

**做什么：** `to_json(const System&, SerializationOptions)` / `from_json(string_view) -> System`，非侵入式，手写。

**文件：**
- `include/sysal/serialization/serialization.hpp` — `SerializationOptions`（pretty_print, include_raw, include_meta）、`to_json`、`from_json`
- `src/serialization/serialize.cpp` — 扩展 System↔JSON（全部 9 域 + meta + warnings + optional raw）。**JSON 结构必须匹配设计文档**：`"info"` 包含全部子系统、`"collect_duration"` 为 double、`"requested_flags"` 为整数。**`from_json` 必须检查 `sysal_version` 兼容性，不兼容时抛 `SysalError`**

**成功标准：** `tests/test_serialization.cpp` 执行 `collect()` → `to_json` → `from_json` → 逐字段比较（meta, cpu 数量, memory, warnings）；断言 `include_raw` 切换 `raw` 字段；版本不匹配抛异常。`utils/check.sh` 全绿。

**Commit：** `feat(serialization): System to_json/from_json`

---

## Phase 10 — Fixtures、回放测试、testbench、收尾 (M)

**做什么：** 真实 fixture、回放测试入口、API 演示、xmake test target、最终全量检查。

**文件：**
- `tests/fixtures/*.json` — 在开发机上运行 `collect(Collect::full|Collect::Raw)` + `save_raw_store` 生成。**可复用 `output/test_raw.json`（10903 行真实数据）作为首个 fixture**。至少一个 fixture
- `tests/test_replay.cpp` — `load_raw_store` fixture → `collect_from_raw` → 断言域不变量（GPU 数量、CPU 数量、可见性）
- `tests/testbench.cpp` — 全量 API 演示：`collect()`、打印 `info.*`、`to_json` pretty、`refresh()`。**修复 A-1（PCI 地址十六进制零填充 `0000:41:00.0`）、A-2（网络速率 Gbps/Mbps）、A-3（存储容量 GiB）显示 bug**
- `xmake.lua` — 重新添加全部 `test_target` 行
- `docs/devlog.md` — 按格式记录本次重写
- `docs/issues.md` — 更新 18 个 bug 状态：11 个标记"已通过重写修复"，7 个重新评估

**成功标准：** `xmake build` + 全部 `xmake run test_*` 通过；`utils/check.sh` 全绿（format + tidy + build + tests）；至少一个 raw-replay fixture 断言通过（无需硬件）。

**Commit：** `test: raw replay fixtures, test_replay, testbench; wire xmake test targets`

---

## 风险评估

- **最高：** P8 resolver（可见性计算 + 冲突解决规则覆盖）和 P7b cpu parser（`/proc/cpuinfo` + sysfs 拓扑）。用专项单元测试 + `review-work` 缓解
- **中：** P4 JSON 引擎改编（转义/数字边界）、P7i execution（cgroup v1 vs v2 检测，全部从 RawStore 读取）、P5 fixture 格式稳定性（成为回放契约——尽早冻结）
- **低：** P0-P3、P6、P9、P10（机械化或已充分规格化）
- **横切：** `-Werror` + clang-tidy `WarningsAsErrors: '*'` 意味着每个阶段必须 tidy 干净，不只是编译干净——用 `utils/check.sh` 作为门禁

## 最终成功标准

1. `xmake -r` 构建 `libsysal.a` + 全部测试 binary，零 warning
2. `utils/check.sh` 退出 0（format + tidy + build + tests，**含实际测试执行**）
3. `System::collect()` 在开发机上运行，填充全部 9 个域
4. `collect_from_raw(load_raw_store(fixture))` 无需硬件重现域不变量
5. `to_json`→`from_json` 逐字段往返 `System`
6. 公共 API 完全匹配 `public_api.md`（无 `Expected`、无 `Diagnostics`、无 `*Info`/`*Subsystem` 后缀、扁平 `SystemInfo`）
7. 无 hwloc 引用；`xmake.lua` 无 hwloc 块；CI 无 `libhwloc-dev`
8. `docs/issues.md` 中 18 个 bug 状态已更新

## Commit 策略

- 每个阶段 = 恰好 1 个 commit
- Commit message 前缀：`chore:` / `feat(<area>):` / `test:`
- P7 产出 9 个 commit（每个子阶段一个）
- **排序规则：** 永不提交不可构建的树。P0 的空库可构建；每个后续阶段只添加可编译代码 + 通过的测试
- 删除策略 = 清空（全部删除在 P0），不存在新旧冲突
