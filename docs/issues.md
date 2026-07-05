# sysal 已知问题清单

> 本文档记录 sysal 当前版本（v0.0.1）中所有已识别的问题。
> 每个问题标注类别、严重程度和状态。
> 重写（Phase 1–10）后大部分问题已修复，以下为最终状态。

---

## A. 显示与格式问题（testbench）

### A-1 PCI 地址显示为十进制，应为十六进制零填充

- **严重程度**: 中
- **状态**: 已通过重写修复
- **描述**: 旧 `testbench.cpp` 的 `format_pci()` 使用 `std::to_string()` 输出十进制。重写后 `testbench.cpp` 使用 `snprintf("%04x:%02x:%02x.%x")` 输出十六进制零填充。

### A-2 网口速率单位错误

- **严重程度**: 中
- **状态**: 已通过重写修复
- **描述**: 旧 testbench 使用 `format_bytes()` 格式化速率，导致 Mbps 被当作字节处理。重写后 testbench 直接输出 Mbps（`speed.value / 1000000`）。

### A-3 硬盘容量显示使用 TiB，应使用 GiB

- **严重程度**: 低
- **状态**: 已通过重写修复
- **描述**: 旧 `format_bytes()` 对大容量自动选用 TiB 单位。重写后 `format_memory()` 优先使用 GiB，避免精度丢失。

---

## B. 数据采集 Bug

### B-1 PCI 地址解析使用十进制，应为十六进制

- **严重程度**: 高
- **状态**: 已通过重写修复
- **描述**: `parse_pci_address()` 现在使用 `std::from_chars` base=16 解析 PCI 地址，正确处理含字母的地址（如 `0000:0a:00.0`）。

### B-2 Storage 设备 PCI 地址全部显示 0:0:0.0

- **严重程度**: 高
- **状态**: 部分修复
- **描述**: sysfs reader 采集 `/sys/block/<dev>/device` 符号链接信息，storage parser 尝试解析 PCI 地址。对 NVMe 设备，symlink 指向 NVMe 控制器而非 PCI 设备节点，需进一步向上追溯 symlink 链。当前 NVMe 设备的 PCI 地址仍可能缺失，parser 会发出警告。

### B-3 GPU PCI 地址显示 0:0:0.0

- **严重程度**: 高
- **状态**: 已通过重写修复
- **描述**: nvidia-smi CSV 的 PCI 地址解析已修正，`parse_pci_address()` 使用十六进制解析，含字母的 bus/device 不再返回全零。

---

## C. 架构问题

### C-1 Pipeline 层边界破坏

- **严重程度**: 高
- **状态**: 已通过重写修复
- **描述**: 所有 syscall 和文件 I/O 已移至 Reader 层。Parser 层仅从 RawStore 读取数据，不再直接调用 `getpid()`、`getuid()`、`getenv()`、`gethostname()`、`getifaddrs()` 等。Raw replay 对全部子域生效。

### C-2 Resolver 职责过重

- **严重程度**: 中
- **状态**: 已通过重写修复
- **描述**: Resolver 简化为 `resolve(ParseResult, warnings) → SystemInfo`，仅做移动组装、可见性计算与冲突检测，不再承担 MetaBuilder 等额外职责（Meta 由 Pipeline 构建）。

### C-3 Pipeline 重复分派模式

- **严重程度**: 中
- **状态**: 已通过重写修复
- **描述**: Pipeline 使用 Collect 位掩码分派，按域调用 9 个解析器。新增子域只需在 Pipeline 和 ParseResult 各加一处。

### C-4 冲突解决未实现

- **严重程度**: 低
- **状态**: 已通过重写修复
- **描述**: Diagnostics 已移除，改为 `std::vector<std::string> warnings`。冲突以 `[conflict] <field>: <src1>=<val>, <src2>=<val>, adopted=<src>` 格式记录到 warnings。v0.0.1 大多单来源，冲突框架就绪。

---

## D. 数据空白（已设计但未实现）

### D-1 CPU 频率未采集

- **严重程度**: 中
- **状态**: 已通过重写修复
- **描述**: sysfs reader 读取 `cpufreq/base_frequency` 和 `cpufreq/scaling_max_freq`，CPU parser 填充 `CpuPackage::base_frequency` 和 `max_frequency`。

### D-2 CPU NUMA 归属未填充

- **严重程度**: 中
- **状态**: 已通过重写修复
- **描述**: sysfs reader 读取 `/sys/devices/system/node/nodeN/cpulist`，CPU parser 反向映射填充 `CpuCore::numa_node` 和 `LogicalCpu::numa_node`。

### D-3 NumaRelation::packages 未填充

- **严重程度**: 中
- **状态**: 已移除
- **描述**: `TopologyInfo` 及 `NumaRelation` 已在重写中移除。NUMA 信息通过 `Cpu::numa_nodes` 和 `Memory::numa_memory` 表达。

### D-4 Accelerator NUMA 亲和未填充

- **严重程度**: 中
- **状态**: 已通过重写修复
- **描述**: sysfs reader 读取 PCI 设备的 `numa_node`，accelerator parser 通过 PCI 地址查找 NUMA 节点填充 `AcceleratorDevice::nearest_numa_node`。

### D-5 PCI parent-child 关系未采集

- **严重程度**: 低
- **状态**: 已移除
- **描述**: `TopologyInfo` 及 `PciRelation` 已在重写中移除。v0.0.1 不提供 PCI 拓扑关系图。

### D-6 Firmware/BIOS 未采集

- **严重程度**: 低
- **状态**: 已通过重写修复
- **描述**: sysfs reader 读取 `/sys/class/dmi/id/` 下 6 个 DMI 文件，platform parser 填充 `Platform::firmware`。

### D-7 虚拟化检测未实现

- **严重程度**: 低
- **状态**: 已通过重写修复
- **描述**: 三源检测硬件虚拟化：`/sys/hypervisor/type`（Xen）→ DMI sys_vendor/product_name 关键词匹配（VMware/Hyper-V/QEMU/VirtualBox/Parallels/KVM）→ `/proc/cpuinfo` flags 含 `hypervisor` 标志（Other）。容器检测独立于硬件虚拟化，由 `ExecutionContext.container` 承载。

### D-8 软件栈大面积空白

- **严重程度**: 中
- **状态**: 部分修复
- **描述**: NVIDIA 驱动版本和 CUDA runtime 版本已实现。`compilers`、`libraries`、`rocm`、`level_zero`、`mpi`、`rdma` 仍为空，推迟到后续版本。

---

## E. 代码组织问题

### E-1 software_parser 过于混杂

- **严重程度**: 中
- **状态**: 已通过重写修复
- **描述**: 代码已重写，software parser 职责清晰：解析 NVIDIA 驱动版本、CUDA 版本、nvidia-smi 输出，构建 SoftwareStack。

### E-2 accelerator_parser 名不副实

- **严重程度**: 低
- **状态**: 已通过重写修复
- **描述**: accelerator parser 已重写，当前仅处理 NVIDIA GPU，但命名与 `Accelerators` 聚合类型一致。后续添加 NPU/FPGA 时扩展即可。

### E-3 from_json 不完整

- **严重程度**: 中
- **状态**: 已通过重写修复
- **描述**: `from_json` 已完整实现，反序列化 System 全部字段（info 9 个子域、meta、warnings、raw），并做版本兼容性检查（0.0.x）。

---

## F. 测试问题

### F-1 缺少 parser 级别单元测试

- **严重程度**: 中
- **状态**: 已通过重写修复
- **描述**: 已为全部 9 个域解析器添加 per-parser 单元测试（test_parse_platform、test_parse_cpu、test_parse_memory、test_parse_accelerator、test_parse_storage、test_parse_pci、test_parse_network、test_parse_software、test_parse_execution），共 18 个测试目标。

### F-2 Raw replay 对 3 个子域失效

- **严重程度**: 高
- **状态**: 已通过重写修复
- **描述**: C-1 修复后，所有 syscall 已移至 Reader 层，raw replay 对全部子域生效。`test_replay` 验证回放结果与实时采集一致。
