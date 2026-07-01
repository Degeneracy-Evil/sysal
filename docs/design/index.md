# sysal 设计文档

> 本目录存放 sysal 项目的全部设计文档，按架构层级拆分为独立小文件。
> 每个文件负责一个明确的设计职能，互不重叠。

## 目录结构

```
docs/design/
├── index.md                      ← 本文件（索引）
├── overview.md                   ← 项目定位与核心原则
├── public_api.md                 ← 公共 API 设计
├── data_model/                   ← 数据模型（System 各组成部分）
│   ├── system.md
│   ├── platform.md
│   ├── cpu.md
│   ├── memory.md
│   ├── accelerator.md
│   ├── network.md
│   ├── storage.md
│   ├── pci.md
│   ├── software.md
│   ├── execution.md
│   ├── raw_store.md
│   └── warnings.md
├── architecture/                 ← 内部架构
│   ├── pipeline.md
│   └── backend_strategy.md
├── rules/                        ← 设计规则与约束
│   ├── strong_typing.md
│   ├── conflict_resolution.md
│   └── thread_safety.md
├── testing/                      ← 测试与序列化
│   ├── serialization.md
│   └── raw_replay.md
└── roadmap.md                    ← 版本范围与未来扩展
```

## 文档索引

### 概览层

| 文件 | 职能 |
|---|---|
| [overview.md](overview.md) | 项目定位、核心设计原则（raw-first pipeline）、与其他项目关系、架构总结 |

### 公共 API 层

| 文件 | 职能 |
|---|---|
| [public_api.md](public_api.md) | `System` 类（对象持有模式）、`SystemInfo` 结构、`Collect` 位掩码枚举、公开成员访问 |

### 数据模型层

| 文件 | 职能 |
|---|---|
| [data_model/system.md](data_model/system.md) | `System` 类、`SystemInfo` 结构、`SnapshotMeta` 元数据 |
| [data_model/platform.md](data_model/platform.md) | `Platform`：host / OS / kernel / arch / firmware / virt |
| [data_model/cpu.md](data_model/cpu.md) | `Cpu`：packages / cores / logical CPUs / ISA extensions |
| [data_model/memory.md](data_model/memory.md) | `Memory`：total / available / NUMA 内存分布 |
| [data_model/accelerator.md](data_model/accelerator.md) | `Accelerators`：GPU / NPU / FPGA 设备 |
| [data_model/network.md](data_model/network.md) | `Network`：网卡列表 / 链路状态 / IP / PCI 地址 |
| [data_model/storage.md](data_model/storage.md) | `Storage`：块设备清单 / 容量 / 类型 |
| [data_model/pci.md](data_model/pci.md) | `Pci`：PCI 设备清单 / vendor / class / NUMA |
| [data_model/software.md](data_model/software.md) | `SoftwareStack`：drivers / runtimes / CUDA / ROCm / MPI / RDMA |
| [data_model/execution.md](data_model/execution.md) | `ExecutionContext`：进程环境 / cgroup / cpuset / 可见性索引 |
| [data_model/raw_store.md](data_model/raw_store.md) | `RawStore` / `RawRecord`：原始证据存储、多记录支持 |
| [data_model/warnings.md](data_model/warnings.md) | 采集过程中的警告信息（`std::vector<std::string>`） |

### 内部架构层

| 文件 | 职能 |
|---|---|
| [architecture/pipeline.md](architecture/pipeline.md) | 内部管线 Reader→RawStore→Parser→ParseResult→Resolver→System、源码布局（api/model/reader/parser/resolver/serialization/pipeline） |
| [architecture/backend_strategy.md](architecture/backend_strategy.md) | 后端选择策略：NVML / procfs / sysfs |

### 设计规则层

| 文件 | 职能 |
|---|---|
| [rules/strong_typing.md](rules/strong_typing.md) | 强类型规则：Unit types / StrongId / Value types / Enumerations |
| [rules/conflict_resolution.md](rules/conflict_resolution.md) | 冲突解决策略：source trust order + 分类规则 |
| [rules/thread_safety.md](rules/thread_safety.md) | 线程安全保证（对象持有模式）、无全局 init() |

### 测试层

| 文件 | 职能 |
|---|---|
| [testing/serialization.md](testing/serialization.md) | JSON 序列化：`to_json` / `from_json` 非侵入式设计 |
| [testing/raw_replay.md](testing/raw_replay.md) | Raw replay 测试策略：fixture 采集与回放、`collect_from_raw` |

### 路线图层

| 文件 | 职能 |
|---|---|
| [roadmap.md](roadmap.md) | v0.0.3 实现范围、非目标、未来扩展（缓存内置 / 跨平台 / 拓扑模块） |

## 阅读顺序

初次了解 sysal 设计时，建议按以下顺序阅读：

1. [overview.md](overview.md) — 理解项目定位和核心原则
2. [public_api.md](public_api.md) — 理解公共 API 形态
3. [data_model/system.md](data_model/system.md) — 理解顶层数据模型
4. [data_model/cpu.md](data_model/cpu.md) — 理解 CPU 模型（最重要）
5. [architecture/pipeline.md](architecture/pipeline.md) — 理解内部管线和模块结构
6. [rules/strong_typing.md](rules/strong_typing.md) — 理解类型系统
7. [roadmap.md](roadmap.md) — 理解 v0.0.3 范围
