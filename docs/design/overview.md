# 概览

## 项目定位

**sysal** 是一个用于采集和表示服务器系统信息的 C++ 库。

sysal 默认**不是**独立可执行程序。
sysal **不是**基准测试框架。
sysal **不是**算子调度器。
sysal **不是**性能预测系统。

它的职责是：

```txt
采集真实的系统信息。
将其归一化为类型化数据结构。
通过简单的 API 返回给调用方。
```

sysal 应当返回**事实**，而非**决策**。

例如，sysal 可以返回：

```txt
GPU0 关联到 NUMA node 0（设备级 numa_node 字段）。
NIC mlx5_0 关联到 PCI 地址 0000:ca:00.0。
CUDA 驱动版本可用。
当前进程只能看到 GPU0 和 GPU1。
```

sysal 不应返回：

```txt
GPU0 是 GEMM 的最佳设备。
NIC mlx5_0 应被选用于通信。
本系统性能良好。
```

这些决策属于更高层的项目，例如 opal 或 opbl。

## 核心设计原则

sysal 遵循以下内部管线：

```txt
Reader → RawStore → Parser → ParseResult → Resolver → System
```

核心原则是：

```txt
原始证据优先。
类型化模型其次。
决策永不。
```

所有采集到的信息应首先进入 raw 层。
类型化的系统模型应从原始证据中派生。

此设计可改善：

* 可调试性
* 可测试性
* 可复现性
* 原始数据访问
* 后端可扩展性
* 未来平台支持

## 与其他项目的关系

```txt
sysal:  本系统拥有什么？当前进程能看到什么？
opal:   应当选择哪个算子？
opbl:   算子运行得多快？
```

sysal 与 opal 和 opbl 保持独立。
sysal 可能被 opal 使用，但不包含任何 opal 特定的调度逻辑。

## 架构总结

```txt
sysal::System::collect(flags)
    ↓
Reader 采集原始证据
    ↓
RawStore 记录原始数据
    ↓
Parser 提取 ParseResult（按域划分，无交叉引用）
    ↓
Resolver 合并事实、解决冲突、构建可见性
    ↓
System 对象返回给调用方
```

核心设计原则：

```txt
类型化的系统信息库。
基于原始证据。
后端无关。
对 LSP 友好。
不做调度决策。
```

## API 调用示例

```cpp
// 采集全部信息
sysal::System sys = sysal::System::collect();

// 访问各子系统的类型化模型
const auto& cpu  = sys.info.cpu;
const auto& mem  = sys.info.memory;
const auto& gpus = sys.info.accelerators;

// 仅采集基本子集
sysal::System basic = sysal::System::collect(sysal::Collect::basic);

// 细粒度控制：按位或组合
using namespace sysal;
sysal::System partial = sysal::System::collect(
    Collect::Platform | Collect::Cpu | Collect::Raw
);

// 刷新已有对象
sys.refresh();
```

失败时抛出 `SysalError`，不使用非抛出式 API。
不需要全局 `init()`，后端初始化在 `collect()` 内部自动完成。
