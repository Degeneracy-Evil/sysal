# 线程安全

## 线程安全保证

| 对象 | 线程安全保证 |
|---|---|
| `System::collect()` | 每次调用生成独立快照；Sysal 管线无共享可变状态。使用可选厂商后端时，其进程级行为受后端约束。 |
| `System` 对象 | 公开成员可修改。只有没有并发修改或刷新时，多线程读取 `info` / `meta` / `warnings` / `raw` 才安全。 |
| `System::refresh()` | 非线程安全。同一 `System` 实例不能并发 `refresh()` 和读取。不同 `System` 实例可各自 `refresh()`，互不影响。 |
| 内部 Reader / Parser / Resolver | 无共享可变状态。每次 `collect()` 创建独立实例。 |

## MPI 场景

MPI 多进程场景下，每个进程独立创建自己的 `System` 对象。
进程间无共享状态，无需跨进程同步。典型用法：

```cpp
// 每个 MPI rank 各自采集
auto sys = sysal::System::collect(sysal::Collect::Cpu | sysal::Collect::Accelerator);

// 各 rank 看到的是本节点的系统信息
const auto& cpu = sys.info.cpu;
```

## 实现约束

1. **无全局可变状态**：无全局变量，无静态局部缓存，无全局 `init()`。
2. **Reader 句柄不跨调用复用**：每次 `collect()` / `refresh()` 创建新的文件句柄和后端句柄。
3. **后端初始化生命周期**：NVML 等后端的初始化（如 `nvmlInit`）和清理（如 `nvmlShutdown`）在 `collect()` / `refresh()` 内部配对完成，不跨调用保持。CUDA/HIP/Level Zero 可能保留厂商库内部的进程级初始化状态，因此应在首次采集前设置设备选择环境变量；运行中修改环境后调用 refresh 不保证 runtime 重新选择设备。
4. **共享快照由调用方同步**：公开成员可直接修改，`refresh()` 会替换快照。引用不能跨并发刷新使用；const 引用不阻止其他线程修改原对象。
