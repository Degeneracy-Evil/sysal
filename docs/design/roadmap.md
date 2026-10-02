# 能力与路线图

Sysal 版本唯一来源为 `include/sysal/version.hpp`，当前能力以代码与本文为准。
本轮七项联合开发的交付清单见 [联合推进计划](../development-plan.md)。

## 当前能力

- Linux Reader → RawStore → Parser → Resolver；公共 `System::collect(Collect)` 与 `refresh()`。
- CPU、NUMA、内存/DIMM、PCI、网络/IP、存储/挂载、平台、软件栈、执行上下文。
- CPU affinity/cpuset；cgroup v1/v2 层级 CPU 时间配额、内存上限与当前用量。
- NVIDIA：动态 NVML 设备与 MIG 采集；nvidia-smi 降级与补充。
- AMD / Intel：DRM/sysfs 设备清单；AMD 可选 VRAM / unique_id，设备级 PCI 与 NUMA。
- GPU 索引、UUID、MIG 选择；AMD ROCR/HIP 与 Intel 基本选择器。
- 软件栈：CUDA、ROCm/HIP、Level Zero、编译器、MPI、RDMA，以及 pkg-config 库识别。
- 完整 JSON 序列化、原始证据回放；未知/未采集字段保留缺失语义。
- C++20、Clang 默认工具链；CentOS 7/GCC 提供 glibc 2.17+ 发布包。

可选后端和运行时没有安装时允许降级。支持边界与数据来源见
[后端策略](architecture/backend_strategy.md) 和 [执行上下文](data_model/execution.md)。

## 后续方向

- 在 AMD / Intel 实际硬件上验证驱动与运行时枚举顺序、不同内核属性。
- 扩展 Intel 子设备/复杂 SYCL 选择器，以及按驱动版本精确处理 MIG 枚举限制。
- 非 Linux 平台 Reader、更多厂商 NPU/FPGA 数据路径按实际需求加入。
- 性能评分、基准测试、调度、长期监控、守护进程与 Web API 不在本库职责中。

旧版本的实施记录和审查结论以 Git 历史与 `docs/quality_reports/` 为准。
