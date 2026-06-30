# 线程安全

| 对象 | 线程安全保证 |
|---|---|
| `System::collect()` | 线程安全。并发调用产生独立的 `System` 对象。 |
| `System` 对象 | 构造后不可变。多线程 const 访问 `info` / `meta` / `warnings` / `raw` 成员安全。 |
| `System::refresh()` | 非线程安全。同一 `System` 实例不能并发 refresh 和读取。 |
| 内部 Reader / Parser / Resolver | 无共享可变状态。每次 `collect` 创建独立实例。 |

## 实现约束

1. 无全局可变状态（无全局变量，无静态局部缓存，无全局 `init()`）。
2. Reader 每次调用创建新的句柄（不复用文件句柄或 NVML 句柄）。
3. 后端初始化（如 NVML `nvmlInit`）在 `collect()` 内部按需自动完成，对调用方透明。
4. `System` 对象构造后成员为公开 const 访问，不提供非 const 方法。
