# sysal

C++ 系统信息抽象库。采集服务器硬件与软件信息，归一化为强类型数据结构，一次调用返回采集快照。采集能力独立于 SystemCard 的终端展示选择。

## 快速开始

```bash
xmake                  # 编译静态库 + 动态库 + 示例
xmake run sysal_info   # 编译并运行演示程序，终端输出全部采集结果
```

```cpp
#include "sysal/sysal.hpp"

sysal::System sys = sysal::System::collect();

const auto& cpu  = sys.info.cpu;        // packages, cores, logical_cpus, feature_summary()
const auto& mem  = sys.info.memory;     // total_memory, numa_nodes, dimms
const auto& gpus = sys.info.accelerators;
```

按需采集，用 `Collect` 位掩码控制范围：

```cpp
sysal::System partial = sysal::System::collect(
    sysal::Collect::Cpu | sysal::Collect::Accelerator
);
```

`System` 的公开成员可修改；没有并发修改或刷新时，多线程读取同一快照安全。采集失败抛出 `SysalError`，部分失败记录到 `sys.warnings`。调用 `sys.refresh()` 复用原采集策略重新采集；旧快照没有请求信息时需要显式调用 `collect`。

## 采集策略与证据

```cpp
sysal::CollectOptions options;
options.filesystems = sysal::FilesystemQuery::None;
options.sensors = sysal::SensorDevices::ExcludeDisplay;
auto snapshot = sysal::System::collect(sysal::Collect::Cpu | sysal::Collect::Storage, options);
auto avx2 = snapshot.info.cpu.feature(sysal::CpuFeatureName{"avx2"});
```

默认容量策略仅查询本地和内存文件系统，默认传感器范围为系统公开的全部设备。
额外文件系统查询由调用方明确选择；查询不进入 automount，不追踪挂载路径中的符号链接。
同步文件系统 I/O 不承诺硬超时。

清单完整性通过 `meta.inventory_completeness()` 查询；空清单只有在来源范围完整时才能解释成零。
CPU 能力区分已报告、未报告和未知，缺少 flag 不等于不支持。
存储的旋转属性不证明物理介质；NVMe、ATA、SCSI 与 PCIe、SATA、SAS 等传输方式分别表达。
详细契约见[职责与数据契约](docs/responsibility-migration.md)和[公共 API](docs/design/public_api.md)。

## 数据模型

| 成员 | 类型 | 说明 |
|------|------|------|
| platform | Platform | 主机、OS、内核、架构、固件、虚拟化 |
| cpu | Cpu | 拓扑、逐 CPU 标识与能力、特征并集/共同项查询 |
| memory | Memory | 总量、内存类型、配置速率、NUMA、DIMM 拓扑 |
| accelerators | Accelerators | GPU、NPU、FPGA |
| network | Network | 网卡、链路状态、IP、PCI 地址 |
| storage | Storage | 块设备、分层、用途、协议、传输、挂载与文件系统统计 |
| storage.health | vector<StorageHealthReport> | 只读 NVMe / SMART 报告、明确设备关联与失败原因 |
| pci | Pci | PCI 设备清单 |
| software | SoftwareStack | 驱动、运行时、CUDA、ROCm、MPI、RDMA |
| execution | ExecutionContext | 进程环境、cgroup、cpuset、容器、可见性 |

可见性是 sysal 的特点：不只报告整机有什么，还报告当前进程能看到什么。`ExecutionContext` 记录 cpuset 限制、`CUDA_VISIBLE_DEVICES`、容器检测结果，据此计算可见设备索引。这对 MPI 多进程场景尤其重要——每个 rank 看到的资源可能不同。

## 架构

```
Reader → RawStore → Parser → ParseResult → Resolver → System
```

Reader 从文件、sysfs、命令输出、系统调用采集原始数据。Parser 解析为强类型结构。Resolver 组装为 `System` 并计算可见性。每层职责单一，可独立测试。传入 `Collect::Raw` 保留原始证据，用于 replay 测试。

## 构建

```bash
xmake f -c -y --toolchain=clang  # 默认 Clang；也可显式选择 gcc
xmake          # 编译库与示例，使用工具链默认标准库和 runtime
xmake -r       # 重新编译
xmake test     # 构建并运行 doctest 单元测试和 replay 集成测试
xmake format   # 显式修复 include/src/tests/examples 下的格式
xmake check    # 只验证格式、静态分析、构建与测试
```

项目保留 C++20；debug/release 优化由 xmake 构建模式决定。版本从 `include/sysal/version.hpp` 读取。

显式启用 Git hook：`git config core.hooksPath .githooks`。hook 只检查已暂存的空白和格式，
不会自动修复或重新暂存。`xmake check` 不修改 tracked 文件或 index。
CI 在 push 或 PR 时运行完整检查、非修改断言及 `sysal_info` 冒烟测试。
开发记录通过 Git 提交维护，无需手工开发日志。

### 兼容性构建（CentOS 7 / RHEL 7）

在 CentOS 7 容器中编译，产出 glibc 2.17 兼容的 `.so` / `.a`。覆盖 CentOS 7 / RHEL 7 及所有主流 Linux 发行版。

**前提条件：** 已安装 Docker 并运行。

```bash
bash docker/centos7-build/build.sh
```

脚本流程：构建 Docker 镜像（centos:7 + devtoolset-11 + xmake）→ 容器内编译 → 运行 `sysal_info` 验证可执行。

详见 [CentOS 7 构建文档](docs/centos7-build.md)。

## 项目边界

sysal 只返回事实，不做决策。不做性能评分、基准测试、算子选择、调度策略。不是 hwloc 的替代品。外部库（NVML、ibverbs）可作为后端接入，但后端类型不泄漏到公共 API。

## License

Apache License 2.0
