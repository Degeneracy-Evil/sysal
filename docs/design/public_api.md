# 公共 API 设计

sysal 暴露一个精简且稳定的公共 API。
公共 API 不暴露内部的 reader、parser 或后端库。

## 设计形态

sysal 的公共入口是一个 `System` 类，采用**对象持有模式**而非全局单例：

* `System::collect()` 是静态工厂方法，一次性完成采集，返回一个持有采集快照的 `System` 对象。
* `System` 对象持有该次采集的全部子系统模型、元数据、警告和可选的原始证据。
* `refresh()` 在已有对象上重新采集，替换内部状态。

`System` 对象即该次采集结果的载体与缓存，调用方持有它即可反复读取类型化数据。
适用于 MPI 多进程场景：每个进程独立创建自己的 `System` 对象，无全局状态冲突。

## 不使用全局 init()

sysal **不需要**全局初始化函数（类似 `MPI_Init`）。

原因：
* procfs / sysfs 读取无需初始化。
* NVML 等后端的初始化在 `collect()` 内部按需自动完成，对调用方透明。
* 全局 `init()` 会引入全局可变状态，与"对象持有、无全局状态"原则矛盾。

## Collect 位掩码枚举

采集范围通过 `Collect` 位掩码枚举控制，替代旧有的 builder 类。

```cpp
namespace sysal
{

enum class Collect : uint32_t {
    Platform    = 1 << 0,
    Cpu         = 1 << 1,
    Memory      = 1 << 2,
    Accelerator = 1 << 3,
    Network     = 1 << 4,
    Storage     = 1 << 5,
    Pci         = 1 << 6,
    Software    = 1 << 7,
    Execution   = 1 << 8,
    Raw         = 1 << 9,
    Sensors     = 1 << 10,
    StorageHealth = 1 << 11,
};

// 按位或，组合多个采集域
constexpr Collect operator|(Collect a, Collect b);

// 测试 flags 与 test 是否有任意重叠位
constexpr bool has(Collect flags, Collect test);

// 预设：基本子集
constexpr Collect basic = Collect::Platform
                        | Collect::Cpu
                        | Collect::Memory
                        | Collect::Execution;

// 预设：全部域
constexpr Collect full = /* 所有位置位 */;

}  // namespace sysal
```

`Collect` 是普通枚举值，可按位或组合，无需链式 builder 调用。
`has()` 用于在内部管线中按位测试需要采集哪些域。

## System 类接口

`System` 的成员公开且可修改。共享读取要求没有并发修改；修改或刷新由调用方同步。

```cpp
namespace sysal
{

// 系统信息本体：各子系统的类型化模型
struct SystemInfo
{
    Platform          platform;
    Cpu               cpu;
    Memory            memory;
    Accelerators      accelerators;
    Network           network;
    Storage           storage;
    Pci               pci;
    SoftwareStack     software;
    ExecutionContext  execution;
    Sensors           sensors;
    HardwareHealth    hardware_health;
};

class System
{
public:
    // 静态工厂：执行一次完整采集，默认采集全部域
    // 失败时抛出 SysalError
    static System collect(Collect flags = full);

    // 显式选择只读采集策略
    static System collect(Collect flags, const CollectOptions &options);

    // 复用已知策略刷新；旧快照缺少请求时需要显式 collect
    void refresh();

public:
    SystemInfo                     info;      // 系统信息本体
    SnapshotMeta                   meta;      // 采集元数据
    std::vector<std::string>       warnings;  // 采集过程中的警告
    std::optional<RawStore>        raw;       // 可选原始证据
};

}  // namespace sysal
```

### 访问方式

通过 `sys.info.<子域>` 直接访问，扁平结构，不再分 `resources` 层：

```cpp
const auto& cpu     = sys.info.cpu;
const auto& mem     = sys.info.memory;
const auto& gpus    = sys.info.accelerators;
const auto& network = sys.info.network;
```

元数据、警告、原始证据在 `System` 顶层，与 `info` 同级：

```cpp
sys.meta.collect_duration;
sys.warnings;
sys.raw;
```

### 抽象层级

```
System
├── info                ← 系统信息本体（数据）
│   ├── platform
│   ├── cpu
│   ├── memory
│   ├── accelerators
│   ├── network
│   ├── storage
│   ├── pci
│   ├── software
│   ├── execution
│   ├── sensors
│   └── hardware_health
├── meta                ← 采集元数据
├── warnings            ← 警告信息
└── raw                 ← 原始证据（可选）
```

数据 / 元数据 / 诊断 / 原始证据四层分离，`info` 内部保持扁平不再分 `resources` 层。

## 错误处理

采集失败时抛出 `SysalError`。非法请求、无效策略或对未知采集请求的快照调用 `refresh()` 时，
在发起采集前抛出 `std::invalid_argument`。
sysal **不使用** `Expected<T, E>` 或其他非抛出式结果类型。

调用方若需捕获错误，使用标准 `try / catch`：

```cpp
try {
    sysal::System sys = sysal::System::collect(sysal::full);
    // 使用 sys.info.cpu / sys.info.memory ...
} catch (const sysal::SysalError& e) {
    // 处理采集失败
}
```

部分子域失败不会中断整体采集，而是记录到 `sys.warnings` 中。

## 使用示例

```cpp
// 1) 默认采集全部域
sysal::System sys = sysal::System::collect();

// 2) 使用预设
sysal::System basic = sysal::System::collect(sysal::basic);

// 3) 链式按位组合
using namespace sysal;
sysal::System dispatch = sysal::System::collect(
    Collect::Platform | Collect::Cpu | Collect::Memory
    | Collect::Accelerator | Collect::Network
    | Collect::Software | Collect::Execution
);

// 4) 完整采集（full 已包含 Collect::Raw）
sysal::System with_raw = sysal::System::collect(sysal::full);

// 5) 读取类型化模型
const auto& cpu  = sys.info.cpu;
const auto& mem  = sys.info.memory;
const auto& gpus = sys.info.accelerators;

// 6) 读取警告与元信息
for (const auto& w : sys.warnings) {
    std::cout << "warning: " << w << '\n';
}

// 7) 在已有对象上刷新
sys.refresh();
```

## 设计约束

* 公共 API 不暴露内部 reader、parser、backend。
* `System` 是可修改快照；并发读取只在没有并发修改或刷新时安全。
* `SystemInfo`、`SnapshotMeta` 等结构体成员均为公开，调用方可选择 const 访问。
* `Collect` 是值类型枚举，可复制、可组合，无生命周期约束。
* 预设常量 `basic` / `full` 为 `constexpr`，编译期可用。
* 不需要全局 `init()`，后端初始化在 `collect()` 内部自动完成。

部分失败域由 `meta.failed_collectors` 标记；未请求域与失败域都可能有默认空模型。
判断空库存时必须结合采集状态。`meta.observations` 描述来源状态，域成功不保证字段完整。

StorageHealth 请求只读设备健康查询；库的 full 预设包含这些查询，概要用途宜显式选择域。

## 采集请求与恢复

`CollectOptions` 包含两个类型化策略：

| 选项 | 默认值 | 可选值 |
| --- | --- | --- |
| filesystems | FilesystemQuery::Local | None、Local、AllMounted |
| sensors | SensorDevices::All | All、ExcludeDisplay |

`None` 不请求容量；`Local` 仅查询已分类的本地/内存文件系统；`AllMounted` 允许查询其他已挂载类型，
包含未知类型。伪文件系统和 ramfs 不提供容量查询。所有策略都避开 automount，不追踪符号链接，
通过描述符核对设备号后调用 `fstatvfs`。同步 I/O 可能等待后端，不能将等待超时当作查询已经取消。
`ExcludeDisplay` 只排除具有 PCI 显示类证据的传感器设备；无法确认设备类别时保留数据。
这些策略也适用于 CPU 域依赖的 thermal 读取。

`meta.requested_flags` 是当前采集或回放选择的解析域。`meta.collection_request` 是已知的原始采集
请求，含原始 flags 和 options；保留 RawStore 时，其 `collection_request` 同样记录采集来源。
实时采集两者一致；回放可选择不同解析域，并保留原始请求证据。

`refresh()` 使用当前 `requested_flags` 和保存的原始策略重新采集，成功后整体替换快照。
没有 `collection_request` 的旧快照保持请求未知，`refresh()` 不猜测默认策略，调用方应显式
`System::collect(flags, options)`。`from_json()` 和 `collect_from_raw()` 不执行系统 I/O，
也不依据当前默认策略删除原始证据中的统计值。

## 查询与兼容

`Cpu::feature()` / `feature_summary()` 从逐 CPU 原始 token 与 `features_reported` 按需生成结果。
共同项仅覆盖返回的逻辑 CPU ID 范围，缺少完整报告时为未知。JSON 的 `feature_summary` 为派生输出，
恢复时重新计算，不成为第二份证据。旧 `isa_extensions` 是兼容的首 CPU 采样字段，不能用作全机保证。

存储新接口为 `StorageDevice::classification`，分别表达层级、用途、协议和已报告传输方式；
`rotational` 保持独立。旧 `kind` 和 `layer` 的历史行为隔离在兼容适配器中，新分类不读取它们。
旧 JSON 缺少新字段时保持未知，不根据名称或本机环境补全。

`ReadFailure` 表达读取失败；`SkipReason` 表达未尝试原因。`BudgetExhausted` 是可表达的原因值，
当前未增加通用采集预算或全局超时参数。原始值、来源和可选字段继续保留。新增枚举不改变旧编号。
公共模型变化要求重新编译并匹配头文件与库；不承诺跨版本二进制结构布局兼容。
