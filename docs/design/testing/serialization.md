# Serialization

JSON serialization via non-intrusive free functions in a separate header:

```cpp
// include/sysal/serialization.hpp

namespace sysal
{

struct SerializationOptions
{
    bool pretty_print = false;
    bool include_raw = false;
    bool include_meta = true;
};

std::string to_json(const SystemSnapshot& snapshot,
                    const SerializationOptions& opts = {});

Expected<SystemSnapshot, SysalError> from_json(std::string_view json);

}  // namespace sysal
```

## Design Points

1. Non-intrusive: free functions, no methods on `SystemSnapshot`.
2. Separate header: `#include <sysal/serialization.hpp>` is opt-in.
3. `from_json` supports raw replay testing.
4. `SnapshotMeta::sysal_version` is written to JSON; `from_json` checks compatibility.
5. v0.0.1 may use a lightweight JSON library (e.g. nlohmann/json) or hand-written serialization.
