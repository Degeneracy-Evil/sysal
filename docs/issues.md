# 当前已知边界

以当前代码和 [联合推进计划](development-plan.md) 为准，旧版问题状态可在 Git 历史查阅。

- Linux 是当前支持平台；跨平台 Reader 尚未实现。
- NVIDIA NVML 与 nvidia-smi 两条采集路径可用。MIG 实例与物理 GPU 分开表示。
  MIG 实例的实际 CUDA 枚举数量仍受驱动/CUDA 版本约束，不把设备清单解释成 CUDA runtime 的设备计数。
- AMD / Intel 设备信息来自 DRM/sysfs；可选属性缺失时不伪造显存或产品名称。
  运行时枚举顺序可能不同于 DRM 顺序，索引选择的边界见后端文档。
- Intel 子设备选择、复杂 SYCL 排除表达式尚不提供精确子设备模型。
- cgroup 限制只能涵盖当前挂载命名空间中可读的祖先；命名空间外的隐藏祖先无法采集。
- CPU quota 是共享执行时间预算，与可见逻辑 CPU 数量不同；内存限额也不等于整机内存。
- 编译器、MPI、RDMA 已实现；ROCm、Level Zero 和常用库根据安装工具或 pkg-config 元数据探测。
  没有元数据不意味着软件一定没有安装。
- 设备级 NUMA/PCI 信息可用，不提供完整的关系拓扑图。
