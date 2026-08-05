# sysal v0.0.7 代码质量评审报告

## 评审日期

2026-08-05

## 评审版本

v0.0.7（commit `16c85ff`，含 5 个软件栈/存储/CPU 新实现提交）

## 变更范围（自上次评审）

自 v0.0.5'（脚手架迁移后）新增约 1258 行，主要涉及：

- **storage**: pci_address 修复（`src/parser/storage.cpp`, `src/reader/linux/sysfs.cpp`）
- **software**: 编译器 / MPI / RDMA / CUDA 路径（`src/parser/software.cpp`, `src/reader/linux/procfs.cpp`）
- **cpu**: 缓存 / 调频策略 / 温度（`src/parser/cpu.cpp`, `include/sysal/model/cpu.hpp`, `include/sysal/types/{enums,units}.hpp`）
- 相应测试与 `docs/design/data_model/cpu.md`

## 评分总览

| 维度 | v0.0.5' | v0.0.7 | 权重 | 加权 | 变化 | 说明 |
|------|:---:|:---:|:---:|:---:|:---:|------|
| D1 设计文档忠实度 | 8 | 7 | 15% | 1.05 | ↓1 | raw_store.md 新增 11 个 RawSource 未收录；反序列化枚举上界陈旧 |
| D2 API 优雅度 | 8 | 8 | 15% | 1.20 | — | 稳定；新字段强类型 + 对称序列化 |
| D3 代码一致性 | 9 | 8 | 12% | 0.96 | ↓1 | read_thermal_zones 文档串重复；first_success 未复用于 nvidia/nvcc |
| D4 核心逻辑正确性 | 8 | 7 | 12% | 0.84 | ↓1 | clang/clang++ 子串碰撞；NVMe 分区挂载匹配缺陷 |
| D5 类型安全 | 8 | 7 | 8% | 0.56 | ↓1 | serialize RawSource 上界陈旧（P1）；uint64→uint32 未校验收窄 |
| D6 错误处理健壮性 | 8 | 8 | 10% | 0.80 | — | 缺失工具静默、异常告警清晰 |
| D7 Parser 健壮性 | 9 | 8 | 10% | 0.80 | ↓1 | clang/clang++ 歧义依赖插入序 |
| D8 职责分离 | 9 | 9 | 10% | 0.90 | — | 层分离零违规 |
| D9 测试质量 | 8 | 7 | 8% | 0.56 | ↓1 | NVMe 分区/clang++ 分派/M 后缀解析缺测 |
| **加权总分** | **8** | **8** | **100%** | **7.67** | **—** | **总评 8 / 10** |

加权总分：1.05 + 1.20 + 0.96 + 0.84 + 0.56 + 0.80 + 0.80 + 0.90 + 0.56 = 7.67 → **8**

---

## 各维度详评

### D1: 设计文档忠实度 7/10（↓1）

- **通过**：`cpu.hpp:58-89` 的 `CpuCache`/`ThermalZone` 与 `cpu.md:124-146` 逐字段一致；software/software stack 模型与 `software.md` 一致；"缺失工具不刷 warning、optional 天然静默"原则在 `software.cpp:273-332` 落实。
- **问题**：
  - `docs/design/data_model/raw_store.md:36-42` 仍只列到 `SysHypervisor`，**新增 11 项 RawSource 未收录**（D1 上轮同样点名该文档）。
  - `src/serialization/serialize.cpp:117` RawSource 枚举上界冻结在 v0.0.5（见 D5/P1）。

### D2: API 优雅度 8/10（—）

**通过**：新物理量使用强类型（`CpuCache::size`→`MemorySize`、`ThermalZone::temp`→`Temperature`）；`cpu_cache_to_json/from_json`、`thermal_zone_to_json/from_json` 对称，`Cpu`/`SoftwareStack` 均带 `j.contains()` 向后兼容守卫；字段命名 snake_case 一致。
**轻微**：`sysal_info.cpp:874` breakdown 循环上界仍 `HwinfoOutput`，新增源未枚举；`Cuda::home` 命名偏模糊。

### D3: 代码一致性 8/10（来自 9）

**问题**：
- `src/parser/cpu.cpp:449-454`：`read_thermal_zones` 文档串重复（复制粘贴错误）。
- `src/parser/software.cpp:222-252`：nvidia/nvcc 用手写循环而非同文件已定义的 `first_success`，本文件内习惯不一致。
- `src/parser/cpu.cpp:170,315`：`extract_cpu_number_from_path` 与 `extract_cache_key` 重复"cpu/cpuN"数字扫描逻辑。
**通过**：匿名命名空间 helper + parse_utils 复用、warning 前缀、clang-format 约定均一致。

### D4: 核心逻辑正确性 7/10（来自 8）

**问题**：
- `software.cpp:275`（P1）`first_success(raw, CompilerVersion, "clang")` 子串匹配：`"clang++ --version"` 也含 `"clang"`。仅装 clang++ 的系统会把 clang++ 误标为 clang。当前靠 reader 插入序（procfs.cpp:271 clang 先于 clang++）掩盖。
- `storage.cpp:226-228`（P1）NVMe 分区挂载匹配合判断 `isdigit(df_name[n])`，`"nvme0n1p1"` 在 n=7 处是 `'p'` 非数字 → 分区 NVMe 设备丢失 mount_point/fs_type（常规场景，未测）。
**通过**：编译器 X.Y.Z 提取、MPI 括号提取、缓存大小 K/M/G、存储 PCI 控制器提取均正确。

### D5: 类型安全 7/10（来自 8）

**问题**：
- `serialize.cpp:117`（P1）`validate_enum(..., RawSource::SysHypervisor, "source")` 上界陈旧。新加 11 个源（CompilerVersion=29…SysfsThermal=39，`enums.hpp:122-132`）在反序列化时被拒抛错，破坏 raw 栈 round-trip。追加保持稳定性的约定被遵守，但守卫未更新。
- `cpu.cpp:71,83,91,192,357,389,407,414,564,582` 多处 `parse_uint` 结果无范围校验 `static_cast<uint32_t>`（现实中值小，但未验证）。
**通过**：`parse_uint`/`parse_pci_address`/`parse_cache_size` 全程 nullopt-guarded；`build_numa_mapping` cpulist 循环有 `MAX_CPUS=1024` 兜底防死循环。

### D6: 错误处理健壮性 8/10（—）

**通过**：缺失工具静默（`software.cpp:265,300,317`）；异常告警（`software.cpp:237,258`）；"无软件数据"仅在完全未采集时告警（`software.cpp:378-387`）；`read_command` stderr 重定向 + 空输出→nullopt（`file_utils.hpp:46,65`）；无空 catch / 无崩溃路径。
**轻微**：编译器/MPI 提取失败静默但记录存在（`software.cpp:280-284,304`）；`physical id`/`core id` 解析失败静默 vs `processor` 告警，语义略不一致。

### D7: Parser 健壮性 8/10（来自 9）

**问题**：仅 clang++ 无 clang 时 `first_success` 子串匹配产生伪 clang 条目（同 D4，健壮性下降主因）。`read_cpu_gov` 仅取首个 governor（异构 CPU 只报一个，非崩溃）。
**通过**：`extract_compiler_version` 三数字段校验拒绝 `13.3.0-6ubuntu2`（5 段）与 `garbled`；缓存路径后缀匹配无歧义（`rfind("/size")` 不匹配 `coherency_line_size`）；thermal 按 zone_name 配对，缺 type/temp 仍有效；resolver 用 `value_or(default)` 空域安全。

### D8: 职责分离 9/10（—）

**通过**：Reader 只写 RawStore（`sysfs.cpp:24-35,40-107`；`procfs.cpp:45-56`），Parser 只消费 RawStore + 返回模型/警告（`software.cpp:218`、`cpu.cpp:614`、`storage.cpp:87`），Pipeline 纯编排（`pipeline.cpp:118-137`）；跨域数据共享在 reader 层去重并有注释。新源按 `Collect::` 分派正确（cache/thermal→Cpu，software 命令→Software）。

### D9: 测试质量 7/10（来自 8）

**通过**：编译器格式变体（gcc/clang/gfortran/g++）；缺工具静默（test 6）、畸形编译器静默 vs nvidia-smi 告警（test 9 vs 5）；CUDA home 三分支（test 18-20）；存储 PCI 多段提取 + 字典序（test 2.5）；CPU cache K-suffix + thermal 配对（test 9）；cache/thermal 缺失空（test 10）。均仅断言新字段真实逻辑，非自证。
**缺口**：NVMe 分区挂载匹配（`nvme0n1p1`）无测（test 8 只有 sda2/sda1 + 整盘 `/dev/nvme0n1`）；clang/clang++ 分毫无测；缓存大小仅测 `K` 后缀（`M` 分支未覆盖）；`cpu.caches.size() >= 4` 下限断言偏松。

---

## 关键问题优先级排序

### P0（发布前必须修复）

无。

### P1（应该修复，本版本）

| # | 维度 | 问题 | 位置 |
|---|------|------|------|
| 1 | D5/D1 | RawSource 反序列化枚举上界陈旧，新源（CompilerVersion…SysfsThermal）加载被拒，破坏 raw round-trip | `serialize.cpp:117` |
| 2 | D4/D7 | `first_success` 子串碰撞：`"clang"` ⊂ `"clang++ --version"`，仅装 clang++ 时误标为 clang | `software.cpp:275,289,293` |
| 3 | D4 | NVMe 分区挂载匹配缺陷：`"nvme0n1p1"` 因 `p` 非数字不匹配，分区 NVMe 失 mount_point/fs_type | `storage.cpp:226-228` |

### P2（建议修复，下个版本）

| # | 维度 | 问题 | 位置 |
|---|------|------|------|
| 4 | D1 | raw_store.md 未收录新增 11 个 RawSource 枚举项 | `docs/design/data_model/raw_store.md:36-42` |
| 5 | D3 | `read_thermal_zones` 文档注释重复 | `cpu.cpp:449-454` |
| 6 | D3 | nvidia/nvcc 选择循环未复用 `first_success` | `software.cpp:220-252` |
| 7 | D3 | `extract_cpu_number_from_path` / `extract_cache_key` 数字扫描重复 | `cpu.cpp:170,315` |
| 8 | D5 | `uint32` 收窄未做范围校验 | `cpu.cpp:71…582` |
| 9 | D9 | 缺 NVMe 分区、clang/clang++ 歧义、缓存 `M` 后缀测试 | `tests/unit/test_parse_*.cpp` |
| 10 | D2 | `sysal_info.cpp` breakdown 循环上界仍 `HwinfoOutput` | `sysal_info.cpp:874` |

---

## 版本趋势

| 版本 | D1 | D2 | D3 | D4 | D5 | D6 | D7 | D8 | D9 | 总分 |
|------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| v0.0.2 | 7 | 8 | 8 | 7 | 8 | 8 | 7 | 8 | 7 | 8 |
| v0.0.4 | 9 | 8 | 8 | 7 | 7 | 7 | 8 | 9 | 8 | 8 |
| v0.0.5 | 8 | 8 | 9 | 8 | 8 | 7 | 9 | 9 | 8 | 8 |
| v0.0.5' | 8 | 8 | 9 | 8 | 8 | 8 | 9 | 9 | 8 | 8 |
| **v0.0.7** | 7 | 8 | 8 | 7 | 7 | 8 | 8 | 9 | 7 | **8** |

新增功能（软件栈/缓存/温度/CUDA）整体质量良好，总分维持 8 分基线。主要失分在新代码引入的三项 P1 正确性缺陷：
1. raw 反序列化枚举上界陈旧（破坏向后兼容）；
2. clang/clang++ 子串匹配歧义；
3. NVMe 分区挂载匹配缺陷。

修复这三项（并顺带更新 raw_store.md、补对应测试）后，D1/D4/D5/D9 有望各回升至 8，总分可冲 8-9 分档。
