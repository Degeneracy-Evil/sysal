# ExecutionContext

描述当前进程的限制与环境：进程是谁、环境变量有哪些、cgroup / cpuset 约束
如何、权限如何、是否在容器内。`ExecutionContext` 回答
"当前进程能看到什么、能用什么"——同一台机器，不同进程的视图可能完全不同。

## ExecutionContext 结构体

```cpp
namespace sysal
{

struct ExecutionContext
{
    Process     process;     // 当前进程
    Environment environment; // 环境变量
    Cgroup      cgroup;      // cgroup 约束
    Cpuset      cpuset;      // cpuset 约束
    Permission  permission;  // 权限
    std::optional<Container> container;  // 容器（若在容器内）

    // 可见性便利索引：当前进程实际能看到的资源 ID
    std::vector<LogicalCpuId>    visible_logical_cpu_ids;         // 可见逻辑 CPU ID
    std::vector<AcceleratorId>   visible_accelerator_ids;         // 可见加速器 ID
    std::vector<InterfaceName>   visible_network_interface_names; // 可见网络接口名
};

}  // namespace sysal
```

## 可见性说明

进程不一定看到整台机器。cgroup、cpuset、容器、设备 cgroup、命名空间等
机制都会限制进程可见的资源范围：

* cpuset 限制进程可用的 CPU 与内存节点。
* 容器可能只暴露部分设备。
* 设备 cgroup 限制可访问的设备文件。
* 网络命名空间限制可见的网络接口。

因此 `Cpu` / `Accelerators` / `Network` 等资源子域描述的是**机器本身**，
而 `ExecutionContext` 描述的是**当前进程的视图**。

## 可见性模型

| 资源 | 可见性判定依据 |
|---|---|
| 逻辑 CPU | `Cpuset` 约束的 CPU 列表 / cgroup v2 `cpuset.cpus.effective` |
| 加速器（GPU） | 设备 cgroup / 容器设备挂载 / `CUDA_VISIBLE_DEVICES` 等环境变量 |
| 网络接口 | 网络命名空间 / 容器网络配置 |
| 内存节点 | `Cpuset` 约束的 mems 列表 |

## 可见性索引的定位

每个资源类型自身有 `visible_to_current_process` 字段作为**事实来源**，
`ExecutionContext` 中的 `visible_logical_cpu_ids` / `visible_accelerator_ids` /
`visible_network_interface_names` 只是**便利索引**——把分散在各资源子域中的
可见性信息汇总到一处，便于上层一次性获取"当前进程能看到哪些资源"，
而不必遍历各子域逐项过滤。

当二者出现不一致时，以各资源子域中的 `visible_to_current_process` 为准，
并将差异记录到 `sys.warnings`。

## 子结构体示例

| 结构体 | 说明 | 示例字段 |
|---|---|---|
| `Process` | 当前进程 | `pid`、`ppid`、`uid`、`gid`、`comm`、`exe`、`cwd` |
| `Environment` | 环境变量 | `entries`（`std::vector<std::pair<std::string, std::string>>`） |
| `Cgroup` | cgroup 约束 | `version`（v1/v2）、`path`、`controllers` |
| `Cpuset` | cpuset 约束 | `cpus`、`mems`、`cpus_effective`、`mems_effective` |
| `Permission` | 权限 | `euid`、`egid`、`capabilities`、`is_root`（bool） |
| `Container` | 容器 | `kind`（Docker/Podman/LXC/...）、`id`、`runtime` |

## 设计说明

* `ExecutionContext` 描述"当前进程"，不描述"机器"——机器视角由
  `Platform` 和各资源子域负责。
* `container` 为 `std::optional`：不在容器内时为 `std::nullopt`。
* 可见性索引是便利字段，事实来源在各资源子域的 `visible_to_current_process`。
* 字段命名遵循 `snake_case`，类型名遵循 `PascalCase`。
