# 归并与可见性策略

## 当前实现（0.0.9）

Parser 将 GPU 库存按来源顺序归并：NVML → nvidia-smi / MIG 列表 →
runtime 可见设备 → DRM。已取得的 UUID、显存和 NUMA 信息保留，后续来源补缺。
物理设备通过 UUID / PCI / 同厂商物理索引关联；MIG 实例优先 UUID，不能仅按 PCI 合并。
尚未实现通用的逐字段冲突报告框架；原始证据可用于审查差异。

Resolver 移动各子域到 SystemInfo，并计算资源可见性：

| 资源 | 依据 |
| --- | --- |
| CPU | 当前进程 CPU affinity / cpuset 的便利索引；旧快照空索引使用既有默认 |
| GPU | 完整的 runtime 进程枚举优先；缺失时用各厂商环境变量；无显式限制默认可见 |
| Network | 当前网络命名空间中采集到的接口 |

GPU 变量未设置与显式空值不同，空值不能按“全部可见”处理。
MIG 存在时物理父设备不重复计入可见计算设备。Resolve 后重新生成 GPU 可见 ID 索引。
索引引用了不存在的资源时记录 warning；CPU affinity 与 CPU quota 不合并为一个数字。

后端缺失、枚举不完整或关联失败时保持降级信息，不利用部分 runtime 列表排除其他设备。
复杂 SYCL 选择器、驱动不同版本的 MIG 枚举规则见 docs/issues.md。
