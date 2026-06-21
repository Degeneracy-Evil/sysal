# RawStore

Stores raw evidence collected from the system.

```cpp
enum class RawSource
{
    // Linux (v0.0.1)
    ProcCpuInfo,
    ProcMemInfo,
    SysfsCpu,
    SysfsNet,
    SysfsPci,
    Lspci,
    HwlocXml,
    HwinfoOutput,
    Nvml,
    NvidiaSmi,
    Ibverbs,
    Lsblk,
    // Windows / macOS — future
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
    std::string path_or_command;           // secondary key
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

One `RawSource` may have multiple records (e.g. many sysfs files under `SysfsCpu`).
`path_or_command` serves as the secondary key for fine-grained access.

`RawStore` is optional in `SystemSnapshot`, enabled via `CollectSpec::with_raw()`.
