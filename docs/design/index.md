# sysal Design Documents

> 本目录存放 sysal 项目的全部设计文档，按架构层级拆分为独立小文件。
> 每个文件负责一个明确的设计职能，互不重叠。

## 目录结构

```
docs/design/
├── index.md                      ← 本文件（索引）
├── overview.md                   ← 项目定位与核心原则
├── public_api.md                 ← 公共 API 设计
├── data_model/                   ← 数据模型（SystemSnapshot 各组成部分）
│   ├── system_snapshot.md
│   ├── platform_info.md
│   ├── resource_info.md
│   ├── topology_info.md
│   ├── software_stack_info.md
│   ├── execution_context.md
│   ├── raw_store.md
│   └── diagnostics.md
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
| [public_api.md](public_api.md) | `collect` / `collect_or_throw` 入口、`CollectSpec` builder + preset、使用示例 |

### 数据模型层

| 文件 | 职能 |
|---|---|
| [data_model/system_snapshot.md](data_model/system_snapshot.md) | `SystemSnapshot` 顶层结构 + `SnapshotMeta` 元数据 |
| [data_model/platform_info.md](data_model/platform_info.md) | `PlatformInfo`：host / OS / kernel / arch / firmware / virt |
| [data_model/resource_info.md](data_model/resource_info.md) | `ResourceInfo`：CPU / Memory / Accelerator / Network / PCI / Storage |
| [data_model/topology_info.md](data_model/topology_info.md) | `TopologyInfo`：NUMA / PCI 关系图、设备亲和性、与 PciSubsystem 分工 |
| [data_model/software_stack_info.md](data_model/software_stack_info.md) | `SoftwareStackInfo`：drivers / runtimes / CUDA / ROCm / MPI / RDMA |
| [data_model/execution_context.md](data_model/execution_context.md) | `ExecutionContextInfo`：进程环境 / cgroup / cpuset / 可见性索引 |
| [data_model/raw_store.md](data_model/raw_store.md) | `RawStore` / `RawRecord`：原始证据存储、多记录支持 |
| [data_model/diagnostics.md](data_model/diagnostics.md) | `Diagnostics` / `Diagnostic` / `ConflictDetail`：采集问题与冲突记录 |

### 内部架构层

| 文件 | 职能 |
|---|---|
| [architecture/pipeline.md](architecture/pipeline.md) | 内部管线 Reader→RawStore→Parser→ParsedFacts→Resolver→SystemSnapshot、源码布局 |
| [architecture/backend_strategy.md](architecture/backend_strategy.md) | 后端选择策略：hwloc / NVML / ibverbs / procfs / sysfs |

### 设计规则层

| 文件 | 职能 |
|---|---|
| [rules/strong_typing.md](rules/strong_typing.md) | 强类型规则：Unit types / StrongId / Value types / Enumerations |
| [rules/conflict_resolution.md](rules/conflict_resolution.md) | 冲突解决策略：source trust order + 分类规则 |
| [rules/thread_safety.md](rules/thread_safety.md) | 线程安全保证与实现约束 |

### 测试层

| 文件 | 职能 |
|---|---|
| [testing/serialization.md](testing/serialization.md) | JSON 序列化：`to_json` / `from_json` 非侵入式设计 |
| [testing/raw_replay.md](testing/raw_replay.md) | Raw replay 测试策略：fixture 采集与回放、`collect_from_raw` |

### 路线图层

| 文件 | 职能 |
|---|---|
| [roadmap.md](roadmap.md) | v0.0.1 实现范围、非目标、未来扩展（缓存 / 跨平台） |

## 阅读顺序

初次了解 sysal 设计时，建议按以下顺序阅读：

1. [overview.md](overview.md) — 理解项目定位和核心原则
2. [public_api.md](public_api.md) — 理解公共 API 形态
3. [data_model/system_snapshot.md](data_model/system_snapshot.md) — 理解顶层数据模型
4. [data_model/resource_info.md](data_model/resource_info.md) — 理解资源模型（最重要）
5. [architecture/pipeline.md](architecture/pipeline.md) — 理解内部管线和模块结构
6. [rules/strong_typing.md](rules/strong_typing.md) — 理解类型系统
7. [roadmap.md](roadmap.md) — 理解 v0.0.1 范围
