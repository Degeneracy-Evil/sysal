# ExecutionContextInfo

Describes restrictions and environment of the current process.

```cpp
struct ExecutionContextInfo
{
    ProcessInfo process;
    EnvironmentInfo environment;
    CgroupInfo cgroup;
    CpusetInfo cpuset;
    PermissionInfo permissions;
    std::optional<ContainerInfo> container;

    // pre-computed visible resource IDs (convenience index)
    std::vector<LogicalCpuId> visible_logical_cpu_ids;
    std::vector<AcceleratorId> visible_accelerator_ids;
    std::vector<InterfaceName> visible_network_interface_names;
};
```

This section is important because the current process may not see the full machine.

```txt
Host has 8 GPUs.
Current process sees only 2 GPUs.

Host has 192 logical CPUs.
Current process is restricted to 32 logical CPUs.
```

Upper-layer libraries should usually use visible resources rather than physical resources.

## Visibility Model

Every resource type carries a `visible_to_current_process` flag (source of truth).
`ExecutionContextInfo` provides pre-computed ID lists for quick lookup (convenience).

| Resource | Visibility determined by |
|---|---|
| CPU | cpuset / cgroup restrictions |
| GPU | `CUDA_VISIBLE_DEVICES` / `HIP_VISIBLE_DEVICES` / cgroup device controller |
| Network | network namespace isolation / cgroup |
| Memory | follows CPU visibility (NUMA node has visible CPUs) |
