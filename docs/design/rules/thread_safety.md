# Thread Safety

| Object | Guarantee |
|---|---|
| `collect(spec)` | Thread-safe. Concurrent calls produce independent snapshots. |
| `collect_or_throw(spec)` | Thread-safe. |
| `SystemSnapshot` | Immutable after construction. Multi-threaded const access is safe. |
| `CollectSpec` | Builder methods are non-const; same instance must not be used concurrently. Const read after building is safe. |
| Internal Reader / Parser / Resolver | No shared mutable state. Each `collect` creates independent instances. |

## Implementation Constraints

1. No global mutable state (no global variables, no static-local caches).
2. Readers create fresh handles per call (no reused file handles or NVML handles).
3. `SystemSnapshot` provides only const access to its members.
