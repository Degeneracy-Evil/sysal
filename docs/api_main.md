# sysal API 参考

sysal 是一个 C++ 系统信息抽象库，采集服务器硬件与软件信息，归一化为强类型数据结构。

## 快速开始

```cpp
#include "sysal/sysal.hpp"

sysal::System sys = sysal::System::collect();

const auto& cpu  = sys.info.cpu;
const auto& mem  = sys.info.memory;
const auto& gpus = sys.info.accelerators;
```

按需采集：

```cpp
sysal::System partial = sysal::System::collect(
    sysal::Collect::Cpu | sysal::Collect::Accelerator
);
```

## 核心类型

- **System** — 顶层采集结果载体，静态工厂 `collect()` + `refresh()`
- **Collect** — 位掩码枚举，控制采集范围
- **SystemInfo** — 扁平结构，包含 platform / cpu / memory / accelerators / network / storage / pci / software / execution
- **SysalError** — 采集失败时抛出的异常

## 模块

| 模块 | 头文件 | 说明 |
|------|--------|------|
| 平台 | `sysal/model/platform.hpp` | 主机、OS、内核、架构、固件、虚拟化 |
| CPU | `sysal/model/cpu.hpp` | packages、cores、逻辑 CPU、NUMA、ISA 扩展 |
| 内存 | `sysal/model/memory.hpp` | 总量、内存类型、配置速率、NUMA、DIMM 拓扑 |
| 加速器 | `sysal/model/accelerator.hpp` | GPU、NPU、FPGA |
| 网络 | `sysal/model/network.hpp` | 网卡、链路状态、IP、PCI 地址 |
| 存储 | `sysal/model/storage.hpp` | 块设备、容量、类型、挂载点 |
| PCI | `sysal/model/pci.hpp` | PCI 设备清单 |
| 软件栈 | `sysal/model/software.hpp` | 驱动、运行时、CUDA、ROCm、MPI、RDMA |
| 执行上下文 | `sysal/model/execution.hpp` | 进程环境、cgroup、cpuset、容器、可见性 |
| 序列化 | `sysal/serialization/serialization.hpp` | JSON 序列化/反序列化 |

## 设计原则

- **一次采集，不可变结果** — `System` 构造后 const 访问，多线程安全
- **强类型** — `StrongId<T, Tag>` 防止 ID 误用，`ScalarUnit<Tag>` 防止单位误用
- **可见性感知** — 不只报告整机有什么，还报告当前进程能看到什么
- **数据源优先级** — syscall > 文件读取 > 命令执行
- **不暴露内部** — 公共 API 不暴露 reader / parser / backend

## 仓库

- [GitHub](https://github.com/Degeneracy-Evil/sysal)
- License: Apache 2.0
