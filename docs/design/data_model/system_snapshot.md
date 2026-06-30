# System 与 SystemInfo

采集得到的中心数据对象，由 `System` 类持有。

## System 类

`System` 取代了原先的 `collect()` / `collect_or_throw()` 自由函数，采用对象持有模式。
不需要全局 `init()`，后端初始化在 `collect()` 内部自动完成。

```cpp
class System
{
public:
    // 静态工厂：执行一次完整采集，默认采集全部域
    // 失败时抛出 SysalError
    static System collect(Collect flags = Collect::full);

    // 在已有对象上重新采集，替换内部状态
    // 失败时抛出 SysalError
    void refresh();

public:
    SystemInfo                     info;      // 系统信息本体
    SnapshotMeta                   meta;      // 采集元数据
    std::vector<std::string>       warnings;  // 采集过程中的警告
    std::optional<RawStore>        raw;       // 可选原始证据
};
```

- 失败直接抛出 `SysalError`，不使用 `Expected<T, E>`。
- 构造完成后 `System` 即为不可变对象（除 `refresh()` 外）。
- `refresh()` 重新采集并替换内部状态，非线程安全。
- `info` / `meta` / `warnings` / `raw` 均为公开成员，直接 const 访问。

## SystemInfo

系统信息本体，包含各子系统的类型化模型。扁平结构，不再分 `resources` 层。

```cpp
struct SystemInfo
{
    PlatformInfo         platform;
    CpuSubsystem         cpu;
    MemorySubsystem      memory;
    AcceleratorSubsystem accelerators;
    NetworkSubsystem     network;
    StorageSubsystem     storage;
    PciSubsystem         pci;
    SoftwareStackInfo    software;
    ExecutionContextInfo execution;
};
```

访问方式：

```cpp
sys.info.cpu;
sys.info.memory;
sys.info.accelerators;
// ...
```

## SnapshotMeta

```cpp
struct SnapshotMeta
{
    std::chrono::system_clock::time_point collect_time;
    std::string sysal_version;             // 例如 "0.0.1"
    std::chrono::milliseconds collect_duration;
    Collect requested_flags;               // 本次采集请求的 Collect 位掩码
    std::vector<std::string> succeeded_collectors;
    std::vector<std::string> failed_collectors;
};
```

| 字段 | 用途 |
|---|---|
| `collect_time` | 快照采集的时间 |
| `sysal_version` | 生成此快照的 sysal 版本 |
| `collect_duration` | 采集所花费的时间 |
| `requested_flags` | 本次采集请求的 `Collect` 位掩码 |
| `succeeded_collectors` | 采集成功的 collector 名称 |
| `failed_collectors` | 采集失败的 collector 名称 |

`requested_flags` 为 `Collect` 位掩码类型，记录本次采集实际请求的内容范围。
