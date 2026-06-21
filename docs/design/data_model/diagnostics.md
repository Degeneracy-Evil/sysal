# Diagnostics

Records collection and parsing issues without necessarily failing the whole collection.

```cpp
enum class Severity
{
    Info,
    Warning,
    Error,
};

struct ConflictDetail
{
    std::string field;                     // e.g. "gpu_memory_size"
    std::string value_from_higher;         // adopted value
    std::string value_from_lower;          // discarded value
    RawSource higher_source;
    RawSource lower_source;
};

struct Diagnostic
{
    Severity severity;
    std::string message;
    std::optional<RawSource> source;
    std::optional<ConflictDetail> conflict;
};

struct Diagnostics
{
    std::vector<Diagnostic> records;
};
```

Partial information is acceptable when some collectors fail.

Examples:

```txt
/proc/cpuinfo reports 192 logical CPUs, but cpuset exposes only 32.
NVML is unavailable.
hwloc topology collection failed, falling back to sysfs.
RDMA device found but no associated netdev was resolved.
```
