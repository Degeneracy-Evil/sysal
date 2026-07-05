# sysal

sysal 是一个 C++ 系统信息抽象库。它采集服务器的硬件与软件信息，归一化为强类型数据结构，一次调用返回不可变对象。

## 为什么需要

Linux 服务器上的系统信息散落在 /proc、/sys、lspci、nvidia-smi、df、udevadm 等各处，每种来源有不同的格式、错误模式和边界情况。sysal 把这些采集和解析逻辑集中管理，对外提供稳定的类型化接口。调用方不需要关心数据来自 procfs 还是命令输出，也不需要处理格式变化和解析错误。

sysal 只返回事实，不做决策。它不评估性能、不选择算子、不做调度。

## 快速开始

```bash
xmake            # 编译静态库 + 动态库
xmake sysal_info  # 编译并运行演示程序
```

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
    sysal::Collect::Cpu | sysal::Collect::Accelerator | sysal::Collect::Raw
);
```

`System` 构造后不可变，多线程 const 访问安全。采集失败抛出 `SysalError`，部分失败记录到 `sys.warnings`。调用 `sys.refresh()` 重新采集。

## 数据模型

`System` 持有 `info`（系统信息）、`meta`（采集元数据）、`warnings`（警告列表）、`raw`（可选原始证据）。

| 成员 | 类型 | 说明 |
|------|------|------|
| platform | Platform | 主机、OS、内核、架构、固件、虚拟化 |
| cpu | Cpu | packages、cores、逻辑 CPU、ISA 扩展 |
| memory | Memory | 总量、NUMA 分布、DIMM 详情 |
| accelerators | Accelerators | GPU、NPU、FPGA |
| network | Network | 网卡、链路状态、IP、PCI 地址 |
| storage | Storage | 块设备、容量、类型、挂载点 |
| pci | Pci | PCI 设备清单 |
| software | SoftwareStack | 驱动、运行时、CUDA、ROCm、MPI、RDMA |
| execution | ExecutionContext | 进程环境、cgroup、cpuset、容器、可见性 |

可见性是 sysal 的独特价值：它不只报告整机有什么，还报告当前进程能看到什么。`ExecutionContext` 记录 cpuset 限制、`CUDA_VISIBLE_DEVICES`、容器检测结果，据此计算每个 CPU、GPU、网卡对当前进程是否可见。这对 MPI 多进程场景尤其重要。

## 设计

```
Reader → RawStore → Parser → ParseResult → Resolver → System
```

Reader 从文件、sysfs、命令输出、系统调用采集原始数据存入 RawStore。Parser 将原始数据解析为强类型结构。Resolver 组装为最终 `System` 对象并计算可见性。每层职责单一，可独立测试。传入 `Collect::Raw` 可保留原始证据，用于 replay 测试。

## 构建

```bash
xmake          # 编译
xmake -r       # 重新编译
xmake run test_replay  # raw replay 测试
```

CI 在 push 到 main 或 PR 时自动运行 clang-format + clang-tidy + build + tests。

## 项目边界

sysal 不做性能评分、基准测试、算子选择、调度策略、守护进程、监控系统、Web 服务，也不是 hwloc 的替代品。外部库（NVML、ibverbs）可作为后端接入，但后端类型不泄漏到公共 API 中。

sysal 与 opal（算子选择）和 opbl（性能基准）保持独立。

## License

Apache License 2.0
