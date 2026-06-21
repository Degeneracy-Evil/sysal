# Strong Typing Rule

sysal avoids generic string maps in the main data model.

Prefer:

```cpp
struct CpuPackage {
    CpuPackageId id;
    CpuVendor vendor;
    DeviceName model_name;
    uint32_t physical_cores;
    uint32_t logical_threads;
    std::optional<Frequency> base_frequency;
    std::optional<Frequency> max_frequency;
};
```

Not:

```cpp
std::unordered_map<std::string, std::string> info;
```

## Unit Types

```cpp
struct MemorySize  { uint64_t bytes; };
struct Frequency   { uint64_t hz; };
struct Bandwidth   { uint64_t bits_per_second; };
```

## Identifier Types

IDs use a `StrongId<T>` template to prevent mixing:

```cpp
using CpuPackageId  = StrongId<uint32_t>;
using CpuCoreId     = StrongId<uint32_t>;
using LogicalCpuId  = StrongId<uint32_t>;
using NumaNodeId    = StrongId<uint32_t>;
using AcceleratorId = StrongId<uint32_t>;
using StorageId     = StrongId<uint32_t>;
```

`StrongId<T>` is a thin wrapper with explicit construction, equality, and hashing.
Different typedefs are not implicitly convertible to each other or to `T`.

## Value Types

```cpp
struct PciAddress {
    uint16_t domain;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
};
```

`Vendor`, `DeviceName`, `InterfaceName`, `MacAddress`, `IpAddress`, etc.
are strong wrapper types over `std::string` or fixed-size arrays,
defined during implementation.

## Enumerations

```cpp
enum class Architecture { X86_64, AArch64, Riscv64, Other };
enum class InterfaceState { Up, Down, Unknown };
enum class StorageKind { Nvme, Sata, Sas, Other };
```

`IsaExtension` is an enum class listing CPU ISA extensions
(e.g. `Avx2`, `Avx512f`, `Sse42`, `Neon`, `Sve2`).
