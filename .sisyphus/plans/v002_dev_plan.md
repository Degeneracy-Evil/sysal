# v0.0.2 开发计划

## 来源

综合三批审查发现：
1. v0.0.1 质量评审报告（9 维度，总分 8/10，已修复 11 个问题）
2. 代码模式审查（if 链、手写 JSON、resource.cpp 重复、README 过时）
3. testbench 运行审查（13 个问题，7 个库 bug + 6 个 testbench 问题）

---

## Phase R1 — 库实现 bug 修复（高优先级）

### R1a: nvidia-smi CSV 字段顺序错误

**文件**：`src/parser/accelerator.cpp`
**问题**：nvidia-smi 查询列顺序是 `index,name,memory.total,pci.bus_id,driver_version`，但 parser 把 field[2] 当 pci.bus_id、field[3] 当 memory.total——正好反了。
**修复**：交换 field[2] 和 field[3] 的解析逻辑。同时解析 field[4]（driver_version），当前被丢弃。

### R1b: 容器检测误报 Docker

**文件**：`src/parser/execution.cpp`、`src/model/raw_store.cpp`
**问题**：`RawStore::has()` 只检查记录是否存在，不检查 `CollectStatus`。reader 总会插入 `RootDockerenv` 记录（`NotCollected` 状态），导致 `has()` 永远返回 true。
**修复**：`detect_container()` 改为 `raw.get_all(RawSource::RootDockerenv)` + 检查 `status == CollectStatus::Success`。或给 `RawStore` 加 `has_success(RawSource)` 方法。

### R1c: 外部命令 stderr 泄漏

**文件**：`src/reader/linux/file_utils.hpp`、`src/reader/linux/procfs.cpp`
**问题**：`read_command()` 用 `popen()` 执行外部命令，stderr 未重定向。`nvcc` 不存在时 `sh: 1: nvcc: not found` 泄漏到终端。
**修复**：`read_command()` 中将命令包装为 `cmd + " 2>/dev/null"`。适用于 nvcc、nvidia-smi、lspci、lsblk 所有外部命令。

### R1d: Kernel version 直接复制 release

**文件**：`src/parser/platform.cpp`
**问题**：第 111 行 `kernel.version = kernel.release`。`/proc/version` 的完整格式是 `Linux version 5.15.0-91-generic (buildd@...) (gcc...) #101-Ubuntu SMP ...`，version 应该是 `#101-Ubuntu SMP ...` 部分。
**修复**：从 `/proc/version` 中正确解析 version 字符串（`#` 开头的构建信息）。

### R1e: hostname 未采集

**文件**：`src/reader/linux/procfs.cpp`、`src/parser/platform.cpp`
**问题**：hostname 永远为空，reader 从未读取 `/proc/sys/kernel/hostname`。
**修复**：procfs reader 中 `read_procfs()` 的 Platform 分支添加读取 `/proc/sys/kernel/hostname`。parser 中将 payload 设置到 `platform.host.hostname`。

### R1f: 容器检测区分"安装了 Docker"和"在容器内运行"

**文件**：`src/parser/execution.cpp`
**问题**：即便修复了 R1b，`/proc/1/cgroup` 检测逻辑可能仍有误报——机器上安装了 Docker 但程序不在容器内运行。
**修复**：审查 `detect_container()` 的 cgroup 检测逻辑，确保只有真正在容器内运行时才报容器。`/.dockerenv` 存在 + `/proc/1/cgroup` 包含 docker 才确认 Docker，单一信号不足以确认。

---

## Phase R2 — 代码重构（中优先级）

### R2a: if-has(flags) 表驱动重构

**文件**：`src/pipeline/pipeline.cpp`、`src/reader/linux/sysfs.cpp`、`src/reader/linux/procfs.cpp`
**问题**：28 处 `if(has(flags, Collect::Xxx))` 调用，手写分派。
**修复**：
- `pipeline.cpp` run_replay：表驱动 `{Collect flag, parse_fn, ParseResult member}`，参考已有的 `record_collector_status` 模式
- `sysfs.cpp` read_sysfs：表驱动 `{Collect flag, reader_fn}`
- `procfs.cpp` read_procfs：表驱动 + 跨域依赖处理（Cpu 依赖 Platform 等）

### R2b: resource.cpp 模板抽象

**文件**：`src/model/resource.cpp`
**问题**：200 行中 ~80% 是重复模式。12 个函数分为两类：find-by-id（6 个）和 filter-by-predicate（6 个），每个 8-11 行，逻辑完全一致。
**修复**：提取 `find_in(range, member_ptr, key)` 和 `filter_by(range, predicate)` 模板函数。预期从 200 行缩减到 ~80 行。

### R2c: parser if-else 链改查找表

**文件**：`src/parser/cpu.cpp`、`src/parser/execution.cpp`、`src/parser/platform.cpp`、`src/parser/accelerator.cpp`、`src/parser/pci.cpp`、`src/parser/network.cpp`
**问题**：多处 if-else 字段分派链，最长 10 分支。
**修复**：按投入产出比排序：
- cpu.cpp ISA 扩展：`static const` 查找表（最高优先，8 个 if 改 1 个 map + 循环）
- accelerator.cpp 单位解析：查找表（5 分支）
- 其他 parser 的 if-else 链：分支数 ≤6，改 dispatch table 收益有限，暂不强制

### R2d: resolve.cpp value_or 链

**文件**：`src/resolver/resolve.cpp`
**问题**：9 行 `info.xxx = std::move(result.xxx).value_or(Xxx{})` 重复。
**修复**：折叠表达式或辅助函数。低优先级（代码量小，可读性尚可）。

---

## Phase R3 — JSON 库替换（大改动，需评估）

### R3a: 引入 nlohmann/json 替换手写 JSON 引擎

**文件**：`src/serialization/json.hpp`（745 行）、`src/serialization/serialize.cpp`（1942 行）
**问题**：2687 行手写 JSON 代码，是代码库中最大的维护负担。
**方案**：
- xmake.lua 添加 `add_requires("nlohmann_json")`
- 删除 `src/serialization/json.hpp`
- 重写 `serialize.cpp`，用 nlohmann/json 的 ADL 序列化
- 保留 `to_json`/`from_json` 公共 API 不变
- 预期 serialize.cpp 从 1942 行缩减到 ~500 行

**风险**：引入外部依赖，需要 xrepo 包管理器可用。需要验证 CI 环境（GitHub Actions）能正确拉取。
**决策点**：是否接受外部依赖？如果保持零依赖，则改为用宏/模板简化手写代码（改动量大但无新依赖）。

---

## Phase R4 — testbench 重写（中优先级）

### R4a: 重新设计 section 布局，消除重复

**问题**：13 个 section 中大量信息重复（CPU 数量出现 6 次，GPU 信息出现 2 次等）。
**修复**：重新设计输出结构：
- 删除 Section 11（Visibility）——各域已输出 visible 信息
- Section 13（Meta）不重复 Section 1 的 collector 列表
- Section 15/16（Partial/Refresh）只输出差异，不重复全部数据
- 容器信息只出现在 Section 10，不在 Section 2

### R4b: 修复输出格式

| 改动 | 具体内容 |
|------|----------|
| 频率自动分级 | 加 `format_frequency()` 辅助函数，GHz/MHz/kHz |
| Network 状态单独一行 | `Name: eth0` + `State: UP` |
| PCI 减少输出 | 只显示前 5 个 + 总数，尝试显示 device_name |
| Visibility 只统计个数 | 删除逐个列举 |
| 删除 find_* 断言 | Section 3 中 find_package/find_core/find_logical_cpu 移除 |

### R4c: 命令预检

**文件**：`tests/testbench.cpp`
**问题**：testbench 自身不应直接调用外部命令，但 Section 17 用 `collect_from_raw` 测试错误处理是合理的。`nvcc: not found` 的修复在 R1c（库层面）。

---

## Phase R5 — 功能增强（低优先级，可跨版本）

### R5a: Storage HDD/SSD 检测

**文件**：`src/reader/linux/sysfs.cpp`、`src/parser/storage.cpp`、`include/sysal/types/enums.hpp`
**方案**：Reader 读取 `/sys/block/<dev>/queue/rotational`，Parser 用 0/1 区分 SSD/HDD。`StorageKind` 扩展 `Ssd` 和 `Hdd`。

### R5b: ISA 扩展枚举扩展

**文件**：`include/sysal/types/enums.hpp`、`src/parser/cpu.cpp`
**方案**：添加 SSE、SSE2、SSE3、SSSE3、SSE4.1、AES、FMA、F16C、PCLMULQDQ 等常见扩展。

### R5c: PCI device_name 填充

**文件**：`src/parser/pci.cpp`
**方案**：解析 `lspci -nn` 输出（已采集但未解析），用 vendor:device ID 映射到设备名。

### R5d: Container 设计调整

**问题**：`Platform.virtualization.container` 字段语义模糊——是"机器上有容器运行时"还是"当前进程在容器内"？
**方案**：将容器信息从 `Platform.virtualization` 移到 `ExecutionContext.container`（已有此字段）。`Platform.virtualization` 只描述硬件级虚拟化（KVM/Xen/VMware）。

---

## Phase R6 — 文档更新

### R6a: README.md 重写

- 修复 compile_commands.json 生成方式描述（不再自动生成）
- 添加 `xmake testbench` 运行方式
- 添加版本号、双库（libsysal.a/so）说明
- 修正"NVML 可选后端"描述
- 更新 v0.0.1 范围 → v0.0.2

### R6b: AGENTS.md 更新

- 修复 compile_commands.json 描述
- 添加双库 target 结构说明
- 添加 `task("testbench")` 说明
- 添加版本管理（version.hpp）说明

---

## 执行顺序与依赖

```
R1 (库 bug 修复) ──── 无依赖，最高优先
  ├── R1a (nvidia-smi CSV)
  ├── R1b (容器 has() bug)
  ├── R1c (stderr 重定向)
  ├── R1d (kernel version)
  ├── R1e (hostname 采集)
  └── R1f (容器检测逻辑)

R2 (代码重构) ──────── 依赖 R1 完成（避免冲突）
  ├── R2a (if-has 表驱动)
  ├── R2b (resource.cpp 模板)
  ├── R2c (parser 查找表)
  └── R2d (resolve value_or)

R3 (JSON 库) ────────── 独立，但建议在 R2 后
  └── R3a (nlohmann/json)

R4 (testbench 重写) ── 依赖 R1（库 bug 修复后输出才正确）
  ├── R4a (section 布局)
  └── R4b (输出格式)

R5 (功能增强) ────────── 可跨版本
  ├── R5a (HDD/SSD)
  ├── R5b (ISA 扩展)
  ├── R5c (PCI device_name)
  └── R5d (Container 设计)

R6 (文档) ──────────── 最后，反映所有改动
  ├── R6a (README)
  └── R6b (AGENTS.md)
```

## 版本目标

v0.0.2：完成 R1 + R2 + R4 + R6，视情况完成 R3 和 R5。
