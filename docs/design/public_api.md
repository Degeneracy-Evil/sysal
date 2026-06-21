# Public API Design

sysal exposes a small and stable public API.

## Entry Points

```cpp
namespace sysal
{

// Non-throwing: returns Expected<SystemSnapshot, SysalError>
Expected<SystemSnapshot, SysalError> collect(const CollectSpec& spec = {});

// Throwing: throws SysalError on failure
SystemSnapshot collect_or_throw(const CollectSpec& spec = {});

}  // namespace sysal
```

`Expected<T, E>` is a result type holding either `T` or `E`, similar to Rust's `Result<T, E>`.
The implementation may use `tl::expected` or an equivalent custom type.

## CollectSpec

`CollectSpec` uses a builder pattern with preset factory methods.

```cpp
class CollectSpec
{
public:
    // --- preset factories ---
    static CollectSpec basic();
    static CollectSpec full();
    static CollectSpec for_operator_dispatch();

    // --- fine-grained builders (chainable) ---
    CollectSpec& with_raw(bool enabled = true);
    CollectSpec& with_platform(bool enabled = true);
    CollectSpec& with_cpu(bool enabled = true);
    CollectSpec& with_memory(bool enabled = true);
    CollectSpec& with_accelerators(bool enabled = true);
    CollectSpec& with_network(bool enabled = true);
    CollectSpec& with_storage(bool enabled = true);
    CollectSpec& with_pci(bool enabled = true);
    CollectSpec& with_topology(bool enabled = true);
    CollectSpec& with_software_stack(bool enabled = true);
    CollectSpec& with_execution_context(bool enabled = true);

    // --- queries (for internal reader/parser use) ---
    bool keep_raw() const;
    bool collect_platform() const;
    bool collect_cpu() const;
    bool collect_memory() const;
    bool collect_accelerators() const;
    bool collect_network() const;
    bool collect_storage() const;
    bool collect_pci() const;
    bool collect_topology() const;
    bool collect_software_stack() const;
    bool collect_execution_context() const;
};
```

## Preset Semantics

| Preset | Domains |
|---|---|
| `basic()` | platform + cpu + memory + execution_context |
| `full()` | all domains + raw |
| `for_operator_dispatch()` | platform + cpu + memory + accelerators + network + topology + software_stack + execution_context |

## Example Usage

```cpp
auto snapshot = sysal::collect_or_throw(
    sysal::CollectSpec::for_operator_dispatch()
        .with_raw()
);

const auto& resources = snapshot.resources;
const auto& cpu = resources.cpu;
const auto& gpus = resources.accelerators.gpus();
```

Non-throwing style:

```cpp
auto result = sysal::collect(
    sysal::CollectSpec::for_operator_dispatch()
        .with_raw()
);

if (!result) {
    return report_error(result.error());
}

const auto& snapshot = *result;
const auto& resources = snapshot.resources;
```

The public API does not expose internal readers, parsers, or backend libraries.
