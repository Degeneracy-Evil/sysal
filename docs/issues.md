# sysal 已知问题清单

> 本文档记录 sysal 当前版本（v0.0.1）中所有已识别的问题。
> 每个问题标注类别、严重程度和状态。

---

## A. 显示与格式问题（testbench）

### A-1 PCI 地址显示为十进制，应为十六进制零填充

- **严重程度**: 中
- **状态**: 待修复
- **描述**: `testbench.cpp` 的 `format_pci()` 使用 `std::to_string()` 输出十进制，PCI 地址标准格式为十六进制零填充 `DDDD:BB:DD.F`（domain 4 位, bus 2 位, device 2 位, function 1 位）。例如 `0:65:0.0` 应显示为 `0000:65:00.0`。

### A-2 网口速率单位错误

- **严重程度**: 中
- **状态**: 待修复
- **描述**: sysfs `speed` 文件报告的速率单位为 Mbps。Parser 代码 `* 1000000U` 转为 bps（bits per second）存入 `Bandwidth`。但 testbench 使用 `format_bytes()` 格式化，该函数按 1024 除以、以字节为单位。导致 1000 Mbps 显示为 "953 MiB/s"，实际应显示为 "1 Gbps" 或 "1000 Mbps"。

### A-3 硬盘容量显示使用 TiB，应使用 GiB

- **严重程度**: 低
- **状态**: 待修复
- **描述**: `format_bytes()` 对大容量自动选用 TiB 单位，导致精度丢失。3 TiB 实际可能是 3072 GiB 或更精确的值。硬盘容量应统一使用 GiB。

---

## B. 数据采集 Bug

### B-1 PCI 地址解析使用十进制，应为十六进制

- **严重程度**: 高
- **状态**: 待修复
- **描述**: `parse_utils.hpp` 的 `parse_pci_address()` 调用 `parse_uint()`，后者使用 `std::from_chars` 默认 base=10。但 PCI 地址在 sysfs 中是十六进制。对于纯数字地址（如 `0000:65:00.0`）碰巧正确，但含字母的地址（如 `0000:0a:00.0`）会解析失败，返回全零。
- **影响**: 含字母的 PCI 地址（bus/device 为 a-f）会被错误解析为 0。

### B-2 Storage 设备 PCI 地址全部显示 0:0:0.0

- **严重程度**: 高
- **状态**: 待修复
- **描述**: `sysfs_reader.cpp` 对 `/sys/block/<dev>/device` 做 `read_symlink` 后取 `.filename()`。对 NVMe 设备，该 symlink 指向 NVMe 控制器设备（如 `nvme0`），而非 PCI 地址。需要进一步向上追溯 symlink 链找到 PCI 设备节点。
- **影响**: 所有 NVMe 存储设备的 `pci_address` 字段错误。

### B-3 GPU [1] PCI 地址显示 0:0:0.0

- **严重程度**: 高
- **状态**: 待修复
- **描述**: testbench 输出中第二块 A100 GPU 的 PCI 地址为 `0:0:0.0`，明显错误。可能原因：nvidia-smi CSV 中该 GPU 的 PCI 地址格式与 parser 预期不符，或 `parse_pci_address` 解析失败返回默认值。
- **影响**: 第二块 GPU 的 PCI 地址信息丢失。

---

## C. 架构问题

### C-1 Pipeline 层边界破坏

- **严重程度**: 高
- **状态**: 已分析，待修复（P0）
- **描述**: 三个 parser 直接调用 syscall，绕过 Reader 层：
  - `execution_parser.cpp`: `getpid()`, `getuid()`, `getenv()`, `read_file("/.dockerenv")`
  - `platform_parser.cpp`: `gethostname()`
  - `network_parser.cpp`: `getifaddrs()`, `inet_ntop()`
- **影响**: Raw replay 对这三个子域失效——replay 时重新调用 syscall 而非使用原始数据。

### C-2 Resolver 职责过重

- **严重程度**: 中
- **状态**: 已分析，待修复（P4-1）
- **描述**: `resolver.cpp` 的 `resolve()` 函数做 5 件事：fill_meta、move facts、默认可见性、cpuset 可见性、预计算 visible_*_ids。应拆分为 MetaBuilder、SnapshotAssembler、VisibilityResolver。

### C-3 Pipeline 重复分派模式

- **严重程度**: 中
- **状态**: 已分析，待修复（P4-2）
- **描述**: `pipeline.cpp` 有 10 个 `if(spec.collect_X()) { facts.X = parse_X(raw, diag); }` 块。同样的模式在 `resolver.cpp` 的 `fill_meta` 和 `resolve` 中各出现一次。新增子域需改 3-4 处。

### C-4 冲突解决未实现

- **严重程度**: 低
- **状态**: 已分析，待修复（P4-3）
- **描述**: `Diagnostics::ConflictDetail` 已定义但从未填充。多来源数据（hwloc vs sysfs）冲突时无处理。

---

## D. 数据空白（已设计但未实现）

### D-1 CPU 频率未采集

- **严重程度**: 中
- **状态**: 待修复（P1-1）
- **描述**: `CpuPackage::base_frequency` 和 `max_frequency` 始终为空。需读 `/sys/devices/system/cpu/cpuN/cpufreq/base_frequency` 和 `scaling_max_freq`。

### D-2 CPU NUMA 归属未填充

- **严重程度**: 中
- **状态**: 待修复（P1-2）
- **描述**: `CpuCore::numa_node` 和 `LogicalCpu::numa_node` 始终为空。需从 `/sys/devices/system/node/nodeN/cpulist` 反向映射。

### D-3 NumaRelation::packages 未填充

- **严重程度**: 中
- **状态**: 待修复（P1-3）
- **描述**: `NumaRelation::packages` 始终为空向量。需从 cpulist 反向映射 CPU → package。

### D-4 Accelerator NUMA 亲和未填充

- **严重程度**: 中
- **状态**: 待修复（P1-4）
- **描述**: `AcceleratorDevice::nearest_numa_node` 始终为空。需从 `TopologyInfo::device_localities` 按 PCI 地址匹配。

### D-5 PCI parent-child 关系未采集

- **严重程度**: 低
- **状态**: 待修复（P1-5）
- **描述**: `TopologyInfo::pci_relations` 始终为空。需读 sysfs parent symlink。

### D-6 Firmware/BIOS 未采集

- **严重程度**: 低
- **状态**: 待修复（P1-6）
- **描述**: `PlatformInfo::firmware` 始终为空。需读 `/sys/class/dmi/id/`。

### D-7 虚拟化检测未实现

- **严重程度**: 低
- **状态**: 待修复（P1-7）
- **描述**: `PlatformInfo::virtualization` 始终为空。需 `systemd-detect-virt` 或 `/proc/1/environ`。

### D-8 软件栈大面积空白

- **严重程度**: 中
- **状态**: 待修复（P2）
- **描述**: `SoftwareStackInfo` 中 `compilers`、`libraries`、`rocm`、`level_zero`、`mpi`、`rdma` 均未实现。

---

## E. 代码组织问题

### E-1 software_parser 过于混杂

- **严重程度**: 中
- **状态**: 已分析，待拆分
- **描述**: `software_parser.cpp`（211 行）混合 NVIDIA 驱动解析、CUDA runtime 解析、nvidia-smi 解析、设备计数、SoftwareStackInfo 组装。应拆为 `nvidia_software_parser` + `software_parser`（编排器）。

### E-2 accelerator_parser 名不副实

- **严重程度**: 低
- **状态**: 已分析，待重命名
- **描述**: `accelerator_parser.cpp` 只做 NVIDIA GPU，名字暗示通用加速器。应重命名为 `nvidia_parser`。

### E-3 from_json 不完整

- **严重程度**: 中
- **状态**: 待修复
- **描述**: `from_json` 仅反序列化 meta + raw，丢失 platform、resources、software、execution 等 7 个顶层字段。JSON round-trip 不完整。

---

## F. 测试问题

### F-1 缺少 parser 级别单元测试

- **严重程度**: 中
- **状态**: 待实现
- **描述**: 当前只有集成测试（test_collect、test_replay、testbench），无 per-parser 单元测试。parser 的解析逻辑缺乏独立覆盖。

### F-2 Raw replay 对 3 个子域失效

- **严重程度**: 高
- **状态**: 待修复（依赖 C-1）
- **描述**: 因 Pipeline 层边界破坏（C-1），raw replay 对 execution、platform、network 三个子域不生效。replay 时重新调用 syscall 而非使用原始数据。
