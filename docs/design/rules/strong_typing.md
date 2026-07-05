# 强类型规则

sysal 在主数据模型中避免使用泛型字符串映射。所有结构化信息通过强类型表示，
在编译期防止语义不同的值相互误用。

## 核心原则

```cpp
// 正确：每个字段都有明确的语义类型
struct CpuPackage {
    CpuPackageId id;
    Vendor vendor;
    DeviceName model_name;
    uint32_t physical_cores;
    uint32_t logical_threads;
    std::optional<Frequency> base_frequency;
    std::optional<Frequency> max_frequency;
};

// 错误：无类型安全，字段含义靠字符串键名猜测
std::unordered_map<std::string, std::string> info;
```

强类型带来三个好处：
1. 编译器阻止混用（不会把 `MemorySize` 当 `Frequency` 传）
2. IDE / LSP 自动补全
3. API 可发现性——结构体字段即文档

## 单位类型

物理量通过 `ScalarUnit<Tag>` 模板包装，防止不同量纲的 `uint64` 值混用：

```cpp
template <typename Tag> struct ScalarUnit
{
    std::uint64_t value{};
    constexpr bool operator==(const ScalarUnit& other) const;
};

using MemorySize   = ScalarUnit<MemorySizeTag>;    // 字节
using Frequency    = ScalarUnit<FrequencyTag>;     // 赫兹
using Bandwidth    = ScalarUnit<BandwidthTag>;     // 比特每秒
using TransferRate = ScalarUnit<TransferRateTag>;  // MT/s（内存传输速率）
```

不同 `Tag` 实例化为不同类型，不可隐式转换。
`MemorySize` 和 `Frequency` 都是 `uint64` 底层，但赋值给对方会编译报错。

## 标识符类型

ID 使用 `StrongId<T, Tag>` 模板，通过幻影标签防止不同 ID 互相混用：

```cpp
template <typename T, typename Tag> class StrongId
{
public:
    StrongId() = default;
    explicit constexpr StrongId(T value);
    constexpr T value() const;
    constexpr bool operator==(const StrongId& other) const;
};

// 每种 ID 有独立的 Tag 类型
using CpuPackageId  = StrongId<uint32_t, CpuPackageIdTag>;
using CpuCoreId     = StrongId<uint32_t, CpuCoreIdTag>;
using LogicalCpuId  = StrongId<uint32_t, LogicalCpuIdTag>;
using NumaNodeId    = StrongId<uint32_t, NumaNodeIdTag>;
using AcceleratorId = StrongId<uint32_t, AcceleratorIdTag>;
using StorageId     = StrongId<uint32_t, StorageIdTag>;
using DriverId      = StrongId<uint32_t, DriverIdTag>;
using RdmaDeviceId  = StrongId<uint32_t, RdmaDeviceIdTag>;
```

`CpuPackageId` 和 `CpuCoreId` 底层都是 `uint32_t`，但编译器拒绝互相赋值。
`StrongId` 支持显式构造、相等比较和 `std::hash`（可用于 `unordered_map`）。

## 值类型

### PciAddress

PCI 地址是固定结构的聚合体，直接用 `struct`：

```cpp
struct PciAddress
{
    uint16_t domain;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    bool operator==(const PciAddress& other) const;
};
```

### 字符串值类型

语义不同的字符串通过 `NamedString<Tag>` 模板包装：

```cpp
template <typename Tag> struct NamedString
{
    std::string value;
    bool operator==(const NamedString& other) const;
};

// 标签类型
struct VendorTag {};
struct DeviceNameTag {};
struct InterfaceNameTag {};
struct MacAddressTag {};
struct IpAddressTag {};
struct PciClassTag {};
struct MountPointTag {};
struct FilesystemTypeTag {};

using Vendor         = NamedString<VendorTag>;           // 厂商名称
using DeviceName     = NamedString<DeviceNameTag>;        // 设备名称
using InterfaceName  = NamedString<InterfaceNameTag>;     // 网络接口名称
using MacAddress     = NamedString<MacAddressTag>;        // MAC 地址
using IpAddress      = NamedString<IpAddressTag>;         // IP 地址
using PciClass       = NamedString<PciClassTag>;          // PCI 设备类别
using MountPoint     = NamedString<MountPointTag>;        // 挂载点路径
using FilesystemType = NamedString<FilesystemTypeTag>;    // 文件系统类型
```

## 枚举

所有枚举一律使用 `enum class`（作用域枚举），防止枚举值污染命名空间：

```cpp
enum class Arch            { X86_64, AArch64, Riscv64, Other };
enum class InterfaceState  { Up, Down, Unknown };
enum class StorageKind     { Nvme, Ssd, Hdd, Other };
enum class AcceleratorKind { Gpu, Npu, Fpga, Other };
enum class IsaExtension    { Sse, Sse2, Sse3, Ssse3, Sse41, Sse42, Avx, Avx2, Avx512f, Avx512cd, Avx512bw, Avx512dq, Avx512vl, Aes, Fma, F16c, Pclmulqdq };
enum class VirtualizationKind { None, Kvm, Xen, Vmware, Qemu, HyperV, VirtualBox, Parallels, Other };
enum class CgroupVersion { V1, V2 };
enum class ContainerKind { Docker, Podman, Lxc, Kubernetes, Other };
```
