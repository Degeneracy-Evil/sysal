# System 与 SystemInfo

`System` 是 sysal 的顶层采集结果载体，采用对象持有模式：调用方持有一个
`System` 对象即可反复读取该次采集的全部类型化数据。`SystemInfo` 是它内部
的信息本体，`SnapshotMeta` 是采集元数据。

## System 类

```cpp
namespace sysal
{

class System
{
public:
    // 静态工厂：执行一次完整采集，默认采集全部域
    // 失败时抛出 SysalError
    static System collect(Collect flags = Collect::full);

    // 在已有对象上重新采集，替换内部状态
    // 失败时抛出 SysalError
    void refresh();

public:
    SystemInfo              info;      // 系统信息本体
    SnapshotMeta            meta;      // 采集元数据
    std::vector<std::string> warnings; // 采集过程中的警告
    std::optional<RawStore> raw;       // 可选原始证据
};

}  // namespace sysal
```

`System` 采用公开成员而非 accessor 方法——构造后不可变（`refresh()` 除外），
无需封装突变控制。`collect()` 是静态工厂，`refresh()` 在已有对象上重新采集。

## SystemInfo 结构体

`SystemInfo` 是扁平结构，直接包含各子域，不再分 `resources` 中间层：

```cpp
namespace sysal
{

struct SystemInfo
{
    Platform         platform;     // 系统基本标识
    Cpu              cpu;          // CPU 资源
    Memory           memory;       // 内存资源
    Accelerators     accelerators; // 加速器资源（GPU 等）
    Network          network;      // 网络设备
    Storage          storage;      // 存储设备
    Pci              pci;          // PCI 拓扑
    SoftwareStack    software;     // 软件栈
    ExecutionContext execution;    // 当前进程的执行上下文
};

}  // namespace sysal
```

### 访问方式

通过 `sys.info.<子域>` 直接访问：

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

## SnapshotMeta 结构体

```cpp
namespace sysal
{

struct SnapshotMeta
{
    std::chrono::system_clock::time_point collect_time;      // 采集时刻
    std::string                           sysal_version;     // sysal 版本
    std::chrono::duration<double>         collect_duration;  // 采集耗时
    Collect                               requested_flags;   // 请求的采集域
    std::vector<std::string>              succeeded_collectors; // 成功的采集器
    std::vector<std::string>              failed_collectors;    // 失败的采集器
};

}  // namespace sysal
```

## 抽象层级

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

数据 / 元数据 / 诊断 / 原始证据四层分离，`info` 内部保持扁平。

## 设计说明

* `System::collect()` 和 `refresh()` 失败时抛出 `SysalError`，不使用
  `Expected<T, E>` 或其他非抛出式结果类型。
* 不需要全局 `init()`：procfs / sysfs 读取无需初始化，NVML 等后端的初始化
  在 `collect()` 内部按需自动完成，对调用方透明。
* `System` 对象在采集完成后是不可变的（`refresh()` 除外）。
* 部分子域失败不会中断整体采集，而是记录到 `sys.warnings` 中。
