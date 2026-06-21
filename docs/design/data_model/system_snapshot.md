# SystemSnapshot

The central data object returned by `collect()`.

```cpp
struct SystemSnapshot
{
    SnapshotMeta meta;
    PlatformInfo platform;
    ResourceInfo resources;
    SoftwareStackInfo software;
    ExecutionContextInfo execution;
    Diagnostics diagnostics;
    std::optional<RawStore> raw;
};
```

`SystemSnapshot` represents the collected system state at one point in time.
After construction, it is treated as immutable: all members are public for direct access,
but callers should not modify them.

## SnapshotMeta

```cpp
struct SnapshotMeta
{
    std::chrono::system_clock::time_point collect_time;
    std::string sysal_version;             // e.g. "0.0.1"
    std::chrono::milliseconds collect_duration;
    CollectSpec requested_spec;
    std::vector<std::string> succeeded_collectors;
    std::vector<std::string> failed_collectors;
};
```

| Field | Purpose |
|---|---|
| `collect_time` | When the snapshot was taken |
| `sysal_version` | Version of sysal that produced this snapshot |
| `collect_duration` | Time spent collecting |
| `requested_spec` | The `CollectSpec` used for this collection |
| `succeeded_collectors` | Names of collectors that succeeded |
| `failed_collectors` | Names of collectors that failed |
