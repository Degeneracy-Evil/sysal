# v0.0.2 R5 功能增强计划

## 背景

R1-R4-R6 已完成并提交。R5 是 v0.0.2 的最后一批改动，完成后即可发布 v0.0.2。

---

## R5a: Storage HDD/SSD 检测

### 现状
- `StorageKind` 枚举：`Nvme / Sata / Sas / Other`
- `infer_storage_kind()` 仅按设备名前缀推断（nvme→Nvme, sd→Sata, 其他→Other）
- reader 读取 `/sys/block/<dev>/size` 和 `/sys/block/<dev>/device/model`
- **未读取** `/sys/block/<dev>/queue/rotational`

### 改动

**1. `include/sysal/types/enums.hpp`** — 扩展 `StorageKind`：
```cpp
enum class StorageKind
{
    Nvme,  ///< NVMe SSD
    Ssd,   ///< SATA/SAS SSD (rotational=0)
    Hdd,   ///< 机械硬盘 (rotational=1)
    Other  ///< 其他或未知
};
```
移除 `Sata` 和 `Sas`，改为按物理属性分类（SSD vs HDD），而非总线类型。

**2. `src/reader/linux/sysfs.cpp` `read_block_sysfs()`** — 增加读取 rotational：
```cpp
read_sysfs_file(raw, RawSource::SysfsBlock, (dir / "queue" / "rotational").string());
```

**3. `src/parser/storage.cpp`** — 修改推断逻辑：
- 优先从 `rotational` 文件判断：`0` → Ssd，`1` → Hdd
- `nvme` 前缀 → Nvme（NVMe 始终是 SSD）
- 无 rotational 数据时 fallback 到前缀推断

**4. `tests/test_parse_storage.cpp`** — 添加 rotational 测试用例。

**5. 序列化兼容** — `serialize.cpp` 中 `StorageKind` 的 to_json/from_json 需适配新枚举值。因为 nlohmann/json 用枚举名映射，不存在整数兼容问题。

---

## R5b: ISA 扩展枚举扩展

### 现状
- `IsaExtension` 枚举：`Sse42, Avx, Avx2, Avx512f, Avx512cd, Avx512bw, Avx512dq, Avx512vl`（8 个）
- `cpu.cpp` 查找表：`sse4_2, avx, avx2, avx512f, avx512cd, avx512bw, avx512dq, avx512vl`

### 改动

**1. `include/sysal/types/enums.hpp`** — 扩展枚举：
```cpp
enum class IsaExtension
{
    // SSE 系列
    Sse,      ///< SSE
    Sse2,     ///< SSE2
    Sse3,     ///< SSE3
    Ssse3,    ///< SSSE3
    Sse41,    ///< SSE4.1
    Sse42,    ///< SSE4.2
    // AVX 系列
    Avx,      ///< AVX
    Avx2,     ///< AVX2
    Avx512f,  ///< AVX-512 Foundation
    Avx512cd, ///< AVX-512 Conflict Detection
    Avx512bw, ///< AVX-512 Byte/Word
    Avx512dq, ///< AVX-512 Doubleword/Quadword
    Avx512vl, ///< AVX-512 Vector Length
    // 其他常见扩展
    Aes,      ///< AES 指令集
    Fma,      ///< FMA
    F16c,     ///< F16C
    Pclmulqdq ///< PCLMULQDQ
};
```

**2. `src/parser/cpu.cpp`** — 扩展查找表：
```cpp
static const std::vector<std::pair<std::string, IsaExtension>> isa_map = {
    {"sse", IsaExtension::Sse},
    {"sse2", IsaExtension::Sse2},
    {"sse3", IsaExtension::Sse3},
    {"ssse3", IsaExtension::Ssse3},
    {"sse4_1", IsaExtension::Sse41},
    {"sse4_2", IsaExtension::Sse42},
    {"avx", IsaExtension::Avx},
    {"avx2", IsaExtension::Avx2},
    {"avx512f", IsaExtension::Avx512f},
    {"avx512cd", IsaExtension::Avx512cd},
    {"avx512bw", IsaExtension::Avx512bw},
    {"avx512dq", IsaExtension::Avx512dq},
    {"avx512vl", IsaExtension::Avx512vl},
    {"aes", IsaExtension::Aes},
    {"fma", IsaExtension::Fma},
    {"f16c", IsaExtension::F16c},
    {"pclmulqdq", IsaExtension::Pclmulqdq},
};
```

**3. `tests/test_parse_cpu.cpp`** — 添加新扩展的解析测试。

---

## R5c: PCI device_name 填充

### 现状
- `PciDevice.device_name` 字段已存在
- `pci.cpp` 第 107 行：`dev.device_name = DeviceName{trimmed}` — 但这里 `trimmed` 是 `device` 文件的内容（十六进制 device ID，如 `0x1e04`），**不是设备名称**
- reader 从 sysfs 读取 `vendor`、`device`、`class`、`numa_node` 文件
- **未采集** `lspci` 命令输出

### 问题分析
sysfs 的 `device` 文件只提供 device ID，不含人类可读名称。要获取设备名有两种方案：
- **方案 A**：运行 `lspci` 命令获取设备名（需要外部命令，且 lspci 可能未安装）
- **方案 B**：读取 sysfs 的 `uevent` 文件或 `/sys/bus/pci/devices/<addr>/class` 推断设备类型

### 改动（方案 A：lspci）

**1. `include/sysal/types/enums.hpp`** — 新增 RawSource：
```cpp
Lspci,  ///< lspci -nnmm 命令输出
```
追加到 RawSource 末尾（保持枚举值兼容）。

**2. `src/reader/linux/file_utils.hpp` 或 procfs.cpp** — 采集 lspci 输出：
```cpp
read_command(raw, RawSource::Lspci, "lspci -nn");
```
`-nn` 显示 vendor:device ID + 名称。失败时记录 CollectStatus::Failed。

**3. `src/parser/pci.cpp`** — 解析 lspci 输出：
- lspci `-nn` 格式：`0000:41:00.0 VGA compatible controller [0300]: NVIDIA Corporation GP102 [GeForce GTX 1080 Ti] [10de:1b06]`
- 解析出 PCI 地址 + 设备名
- 用 PCI 地址匹配 sysfs 采集的设备，填充 `device_name`

**4. `include/sysal/model/pci.hpp`** — 新增字段：
```cpp
Vendor vendor;        // 已有 — sysfs vendor ID (0x10de)
DeviceName device_name; // 已有 — 改为从 lspci 填充人类可读名称
```
保留 `vendor` 中的 ID，`device_name` 改为 lspci 名称。

**5. `tests/test_parse_pci.cpp`** — 添加 lspci 解析测试。

---

## R5d: Container 设计调整

### 现状
- `Platform.virtualization.container` — bool，"是否为容器"
- `ExecutionContext.container` — `std::optional<Container>`，已有详细类型信息
- `platform.cpp` `detect_virtualization()` 设置 `virt.container = true`
- `execution.cpp` `detect_container()` 设置 `ctx.container = Container{kind, ...}`
- **重复**：两个地方都在检测容器，`Platform.virtualization.container` 是冗余的 bool

### 改动

**1. `include/sysal/model/platform.hpp`** — 从 `Virtualization` 移除 `container` 字段：
```cpp
struct Virtualization
{
    VirtualizationKind kind{}; ///< 虚拟化类型
    std::string hypervisor;    ///< 虚拟机监控程序
    // container 字段移除
};
```

**2. `src/parser/platform.cpp`** — `detect_virtualization()` 移除容器检测逻辑：
- 删除 `virt.container = true` 相关代码（第 228、242 行）
- 只保留 KVM/Xen/VMware 等硬件虚拟化检测

**3. `src/parser/execution.cpp`** — 保持不变（`detect_container()` 已在 ExecutionContext 中）

**4. `src/serialization/serialize.cpp`** — 移除 `Virtualization::container` 的序列化字段。

**5. `tests/test_parse_platform.cpp`** — 移除对 `virtualization.container` 的断言。

**6. 破坏性变更说明** — `Virtualization.container` 是公共 API 字段，移除属于破坏性变更。但 v0.0.2 尚在早期开发阶段，且该字段语义与 `ExecutionContext.container` 重复，可接受。

---

## R5e: 版本号更新

**1. `include/sysal/version.hpp`** — 版本号 `0.0.1` → `0.0.2`

**2. `README.md`** — 版本号更新

**3. `docs/devlog.md`** — 记录所有 R5 变更

---

## 执行顺序

```
R5a (Storage HDD/SSD)  ── 独立
R5b (ISA 扩展)          ── 独立
R5c (PCI device_name)   ── 独立
R5d (Container 设计)    ── 独立
R5e (版本号 + 文档)      ── 依赖 R5a-d 完成
```

R5a-R5d 互相独立，可并行 delegate。R5e 最后执行。

## 委派计划

- R5a + R5d → 一个 agent（都涉及 model + parser + serialize 改动）
- R5b → 一个 agent（简单，快速）
- R5c → 一个 agent（需要新增 RawSource + reader + parser）

最多 2 个 agent 并行（遵守"不要同时开太多agent"约束）。

## 验证

- `xmake -r` build OK
- 全部 test 通过（test_parse_storage, test_parse_cpu, test_parse_pci, test_parse_platform, test_serialization, test_replay, testbench）
- clang-tidy 零 warning（项目代码，不含 third_party）
