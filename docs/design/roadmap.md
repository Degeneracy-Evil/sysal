# 路线图

## v0.0.3 实现范围

### 公共 API

| 组件 | 说明 |
|------|------|
| `System::collect(Collect)` | 静态工厂，采集并返回 `System` 对象 |
| `System::refresh()` | 在已有对象上重新采集 |
| `Collect` 位掩码枚举 | 按域组合采集范围，附 `basic` / `full` 预设 |

不需要全局 `init()`，后端初始化在 `collect()` 内部自动完成。

### 数据模型

| 模型 | 说明 |
|------|------|
| `System` | 顶层载体，持有 `info` / `meta` / `warnings` / `raw` |
| `SystemInfo` | 扁平结构，包含全部子系统 |
| `Platform` | 主机名、OS、内核、架构、固件、虚拟化 |
| `Cpu` | packages / cores / logical CPUs / ISA 扩展 / 可见性 |
| `Memory` | 总量 / 可用量 / NUMA 内存分布 |
| `Accelerators` | GPU / NPU / FPGA 设备列表 |
| `Network` | 网卡列表 / 链路状态 / IP / PCI 地址 |
| `Storage` | 块设备清单 / 容量 / 类型 |
| `Pci` | PCI 设备清单 / vendor / class / NUMA |
| `SoftwareStack` | 驱动 / 运行时 / 编译器 / CUDA / ROCm / MPI / RDMA |
| `ExecutionContext` | 进程环境 / cgroup / cpuset / 容器 / 可见性索引 |
| `warnings` | `std::vector<std::string>`，非致命问题记录 |
| `RawStore` | 多记录原始证据存储 |

### 内部管线

```
Reader → RawStore → Parser → ParseResult → Resolver → System
```

### 平台支持

- **Linux**：procfs、sysfs、PCI、基本网络、基本 CPU 和内存
- **可选后端**：NVML（NVIDIA GPU）

### 测试

- `collect_from_raw` + `load_raw_store` / `save_raw_store` 支持 raw replay
- 无需真实硬件即可测试 Parser / Resolver

### 线程安全

- 无状态 `collect()`，不可变 `System` 对象
- 适用于 MPI 多进程场景

### 首批实现目标

| 子域 | 采集内容 |
|------|----------|
| CPU | 架构、packages、cores、逻辑 CPU（含父 ID）、ISA 扩展、可见 CPU、设备级 `numa_node` |
| Memory | 总量、NUMA 内存 |
| Accelerator | NVIDIA GPU 名称、显存、PCI 地址、可见性、`numa_node` |
| Network | 名称、状态、速率、IP、PCI 地址、可见性 |
| Software | OS、内核、NVIDIA 驱动、CUDA 运行时 |
| Execution | 环境变量、cgroup/cpuset 可见性、容器检测 |
| Raw | 可选原始记录 |
| warnings | 采集 / 解析 / 解决过程中的非致命问题 |

## v0.0.3 非目标

- 性能评分
- 基准测试执行
- 算子选择
- 调度策略
- 长期监控
- 守护进程模式
- 数据库存储
- Web API
- 复杂插件系统
- 完整跨平台支持
- 完整硬件清单
- 拓扑信息（已有 hwloc 等成熟库）
- 全局 `init()` 函数（无此需求）

## v0.0.2 实现范围

| 组件 | 说明 |
|------|------|
| nlohmann/json 序列化 | `to_json` / `from_json` 非侵入式序列化 |
| 表驱动分发 | Reader 和 Parser 改用表驱动模式，替代 if-else 链 |
| 容器检测细化 | Docker、Podman、LXC、Kubernetes 检测 |
| ISA 扩展 | 从 5 项扩展到 17 项（SSE/AVX/AVX-512/AES/FMA 等） |
| lspci 设备名合并 | 将 lspci 输出中的设备名合并到 PCI 模型中 |
| Storage HDD/SSD 检测 | 通过 sysfs rotational 标志区分 HDD 和 SSD |
| UEFI 检测 | 通过 sysfs DMI 检测 UEFI 固件 |

## v0.0.3 实现范围

| 组件 | 说明 |
|------|------|
| 代码审查修复 | 范围限制、CollectStatus 检查、枚举验证、assert→CHECK、死代码移除 |
| testbench→sysal_info 重命名 | 示例程序从 testbench 更名为 sysal_info |
| capabilities 解码 | PCI capabilities 解析 |
| 设计文档对齐 | 全部设计文档与实际代码保持一致 |

## 未来扩展

### 缓存

已内置。`System` 对象本身即采集结果的缓存，调用方持有它即可反复读取。
`refresh()` 是显式的缓存失效与重建入口。无需额外的 `Collector` 类或增量采集接口。

### 跨平台

v0.0.3 仅支持 Linux。扩展路径：

1. `RawSource` 枚举增加平台特定的值（如 `WmiCpu`、`SysctlHw`）
2. Reader 位于 `src/reader/<platform>/` 下，由 xmake 构建时选择
3. 无需修改公共 API——`System` 与平台无关

### 拓扑信息

v0.0.3 不提供 NUMA / PCI 关系图等拓扑抽象，因为已有 hwloc 等成熟库可胜任。
设备级 `numa_node` 字段保留在 `PciDevice`、`AcceleratorDevice`、`CpuCore`、`LogicalCpu` 上，
直接从 sysfs 读取。

未来如需封装更高层的拓扑关系，可作为独立模块引入，不污染核心数据模型。
