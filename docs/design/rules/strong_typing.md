# 强类型规则

sysal 在主数据模型中避免使用泛型字符串映射。

优先使用：

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

而不是：

```cpp
std::unordered_map<std::string, std::string> info;
```

## 单位类型

```cpp
struct MemorySize  { uint64_t bytes; };
struct Frequency   { uint64_t hz; };
struct Bandwidth   { uint64_t bits_per_second; };
```

## 标识符类型

ID 使用 `StrongId<T>` 模板以防止混用：

```cpp
using CpuPackageId  = StrongId<uint32_t>;
using CpuCoreId     = StrongId<uint32_t>;
using LogicalCpuId  = StrongId<uint32_t>;
using NumaNodeId    = StrongId<uint32_t>;
using AcceleratorId = StrongId<uint32_t>;
using StorageId     = StrongId<uint32_t>;
```

`StrongId<T>` 是一个轻量包装类型，具有显式构造、相等比较和哈希支持。
不同的 typedef 之间不会隐式转换，也不会与 `T` 隐式转换。

## 值类型

```cpp
struct PciAddress {
    uint16_t domain;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
};
```

`Vendor`、`DeviceName`、`InterfaceName`、`MacAddress`、`IpAddress` 等
是基于 `std::string` 或固定大小数组的强类型包装，
在实现过程中定义。

## 枚举

```cpp
enum class Architecture { X86_64, AArch64, Riscv64, Other };
enum class InterfaceState { Up, Down, Unknown };
enum class StorageKind { Nvme, Sata, Sas, Other };
```

`IsaExtension` 是一个枚举类，列出 CPU ISA 扩展
（例如 `Avx2`、`Avx512f`、`Sse42`、`Neon`、`Sve2`）。
