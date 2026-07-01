# 公共 API 设计

sysal 暴露一个精简且稳定的公共 API。
公共 API 不暴露内部的 reader、parser 或后端库。

## 设计形态

sysal 的公共入口是一个 `System` 类，采用**对象持有模式**而非全局单例：

* `System::collect()` 是静态工厂方法，一次性完成采集，返回一个不可变的 `System` 对象。
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
};

// 按位或，组合多个采集域
constexpr Collect operator|(Collect a, Collect b);

// 测试 flags 中是否包含 test 位
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

`System` 采用公开成员而非 accessor 方法——构造后不可变，无需封装突变控制。

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
};

class System
{
public:
    // 静态工厂：执行一次完整采集，默认采集全部域
    // 失败时抛出 SysalError
    static System collect(Collect flags = full);

    // 在已有对象上重新采集，替换内部状态
    // 失败时抛出 SysalError
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
│   └── execution
├── meta                ← 采集元数据
├── warnings            ← 警告信息
└── raw                 ← 原始证据（可选）
```

数据 / 元数据 / 诊断 / 原始证据四层分离，`info` 内部保持扁平不再分 `resources` 层。

## 错误处理

`System::collect()` 和 `refresh()` 在失败时抛出 `SysalError`。
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
sysal::System basic = sysal::System::collect(sysal::Collect::basic);

// 3) 链式按位组合
using namespace sysal;
sysal::System dispatch = sysal::System::collect(
    Collect::Platform | Collect::Cpu | Collect::Memory
    | Collect::Accelerator | Collect::Network
    | Collect::Software | Collect::Execution
);

// 4) 同时保留原始证据
sysal::System with_raw = sysal::System::collect(
    full | Collect::Raw
);

// 5) 读取类型化模型
const auto& cpu  = sys.info.cpu;
const auto& mem  = sys.info.memory;
const auto& gpus = sys.info.accelerators;

// 6) 读取警告与元信息
for (const auto& w : sys.warnings) {
    std::println("warning: {}", w);
}

// 7) 在已有对象上刷新
sys.refresh();
```

## 设计约束

* 公共 API 不暴露内部 reader、parser、backend。
* `System` 对象在采集完成后是不可变的（除 `refresh()` 外）。
* `SystemInfo`、`SnapshotMeta` 等结构体成员均为公开，构造后直接 const 访问。
* `Collect` 是值类型枚举，可复制、可组合，无生命周期约束。
* 预设常量 `basic` / `full` 为 `constexpr`，编译期可用。
* 不需要全局 `init()`，后端初始化在 `collect()` 内部自动完成。
