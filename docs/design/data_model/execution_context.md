# ExecutionContextInfo

描述当前进程的限制与环境。

```cpp
struct ExecutionContextInfo
{
    ProcessInfo process;
    EnvironmentInfo environment;
    CgroupInfo cgroup;
    CpusetInfo cpuset;
    PermissionInfo permissions;
    std::optional<ContainerInfo> container;

    // 预计算的可见资源 ID（便利索引）
    std::vector<LogicalCpuId> visible_logical_cpu_ids;
    std::vector<AcceleratorId> visible_accelerator_ids;
    std::vector<InterfaceName> visible_network_interface_names;
};
```

本节很重要，因为当前进程不一定能看到整台机器。

```txt
Host has 8 GPUs.
Current process sees only 2 GPUs.

Host has 192 logical CPUs.
Current process is restricted to 32 logical CPUs.
```

上层库通常应使用可见资源而非物理资源。

## 可见性模型

每个资源类型都携带一个 `visible_to_current_process` 标志（事实来源）。
`ExecutionContextInfo` 提供预计算的 ID 列表，便于快速查找（便利用途）。

| 资源 | 可见性判定依据 |
|---|---|
| CPU | cpuset / cgroup 限制 |
| GPU | `CUDA_VISIBLE_DEVICES` / `HIP_VISIBLE_DEVICES` / cgroup 设备控制器 |
| 网络 | 网络命名空间隔离 / cgroup |
| 内存 | 跟随 CPU 可见性（NUMA 节点含有可见 CPU） |
