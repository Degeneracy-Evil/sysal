# 后端策略

Reader 执行所有系统 I/O 与动态库调用，将证据保存到 RawStore；Parser 和 Resolver
只消费证据，因此离线 replay 不会访问当前机器或初始化 GPU runtime。
公共 API 不暴露厂商库类型，也不要求安装 CUDA、ROCm 或 Level Zero 开发 SDK。

| 数据 | 当前路径 | 降级路径 |
| --- | --- | --- |
| NVIDIA 物理 GPU / MIG | 动态加载 NVML，查询名称、UUID、PCI、显存和实例关联 | nvidia-smi CSV 与 `-L`，再到 DRM |
| NVIDIA 进程可见性 | CUDA Driver API 枚举当前进程设备，按 UUID 关联库存 | CUDA 环境变量；无 runtime 时顺序与 MIG 数量仅为估计 |
| AMD GPU | HIP 枚举 PCI、名称、显存；DRM 补全库存和 NUMA | DRM 的 vendor/device/product_name/VRAM/unique_id |
| AMD 进程可见性 | HIP 实际枚举结果 | ROCR/HIP 环境变量，索引按 DRM 库存估计并发出 warning |
| Intel GPU | Level Zero PCI 枚举与 DRM 库存 | DRM；型号或显存不可读时保留未知值 |
| Intel 进程可见性 | Level Zero 实际枚举；基本 ONEAPI selector | ZE_AFFINITY_MASK 与基本 selector；复杂子设备表达式尚未完整支持 |
| 软件栈 | 命令、NVML 驱动信息、ROCm 版本文件、HIP runtime / Level Zero API、pkg-config | 可用信息独立返回，缺失不等同于未安装 |
| cgroup | mountinfo + 当前进程成员路径，读取 v1/v2 叶节点与可访问祖先 | 不可读字段保持未知，不伪造无限制 |

GPU 库只在所需采集域中按需加载，无可选库时仍可采集其他域。
完整 runtime 枚举才能建立排除集合；查询失败或 UUID/PCI 无法关联时回退到环境变量。
原始 runtime 枚举、DRM、cgroup 文件和包元数据均可随 Collect::Raw 保存。
设备 NUMA 信息来自 sysfs；RDMA、网络、存储与基础硬件仍使用现有 procfs/sysfs 路径。

ROCm 包版本、HIP runtime 版本、Level Zero API 版本分别表达不同信息；不互相冒充。
通用库识别目前使用 pkg-config（OpenCL、BLAS/LAPACK、FFTW、libfabric、hwloc 等），
没有遍历整个文件系统，也没有据此证明某个应用实际加载了这些库。
