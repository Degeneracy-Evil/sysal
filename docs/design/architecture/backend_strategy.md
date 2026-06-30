# 后端策略

sysal 可以依赖外部库，但公共 API 必须保持独立。

```txt
通用硬件信息: procfs + sysfs + PCI
GPU:          NVML（NVIDIA），ROCm SMI（AMD），Level Zero（Intel）
RDMA:         sysfs + ibverbs
软件栈:        版本查询、动态库检测、命令输出解析
```

后端类型不会泄漏到公共 API 中。

NUMA 亲和性信息（设备级 `numa_node`）直接从 sysfs 读取，不依赖 hwloc。
