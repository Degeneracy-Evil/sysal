# RawStore

存储从系统采集的原始证据。

```cpp
enum class RawSource
{
    // Linux（v0.0.1）
    ProcCpuInfo,
    ProcMemInfo,
    SysfsCpu,
    SysfsNet,
    SysfsPci,
    Lspci,
    HwinfoOutput,
    Nvml,
    NvidiaSmi,
    Ibverbs,
    Lsblk,
    // Windows / macOS — 未来支持
};

enum class CollectStatus
{
    Success,
    Partial,
    Failed,
    NotCollected,
};

struct RawRecord
{
    RawSource source;
    std::string path_or_command;           // 次级键
    std::string payload;
    CollectStatus status;
    std::chrono::system_clock::time_point collected_at;
};

struct RawStore
{
    std::vector<RawRecord> records;

    std::vector<const RawRecord*> get_all(RawSource source) const;
    std::vector<const RawRecord*> get(RawSource source,
                                      std::string_view path_or_command) const;
    bool has(RawSource source) const;
    std::size_t count(RawSource source) const;
};
```

一个 `RawSource` 可能对应多条记录（例如 `SysfsCpu` 下有许多 sysfs 文件）。
`path_or_command` 作为次级键，用于细粒度访问。

`RawStore` 在 `SystemSnapshot` 中是可选的。通过在构造 `System` 时
将 `Collect::Raw` 加入请求的 `Collect` 位掩码来启用采集；未设置该标志时
`SystemSnapshot::raw` 为 `std::nullopt`。
