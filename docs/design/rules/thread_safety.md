# 线程安全

## 线程安全保证

| 对象 | 线程安全保证 |
|---|---|
| `System::collect()` | 线程安全。并发调用产生独立的 `System` 对象，无共享状态。 |
| `System` 对象 | 构造后不可变。多线程 const 访问 `info` / `meta` / `warnings` / `raw` 成员安全。 |
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
3. **后端初始化生命周期**：NVML 等后端的初始化（如 `nvmlInit`）和清理（如 `nvmlShutdown`）在 `collect()` / `refresh()` 内部配对完成，不跨调用保持。对调用方完全透明。
4. **`System` 构造后只读**：成员为公开 const 访问，不提供非 const 方法（`refresh()` 除外）。
