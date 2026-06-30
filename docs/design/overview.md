# 概览

## sysal 是什么

sysal 是一个 C++ 系统信息抽象库。它采集服务器的硬件与软件信息，归一化为强类型数据结构，
通过简洁的 API 交给调用方使用。

sysal 只回答一个问题：**这台机器上有什么，当前进程能看到什么。**

它不回答：哪个设备最好、哪个算子该选、系统性能如何。那些是上层项目（opal、opbl）的职责。

## 设计哲学

**原始证据优先，类型化模型其次，决策永不。**

所有信息先以原始形态进入 `RawStore`（/proc 文件内容、sysfs 属性、命令输出等），
再由 Parser 解析为结构化类型，最后由 Resolver 组装为 `System` 对象。
原始数据始终可追溯，便于调试与测试。

这条原则带来的好处：

- **可调试**：解析结果有误时，可直接检查原始证据
- **可测试**：无需真实硬件，用保存的原始数据即可回放测试
- **后端无关**：更换数据来源（procfs → hwloc → NVML）不影响公共 API
- **跨平台友好**：新增平台只需新增 Reader，类型化模型不变

## 内部管线

```
Reader → RawStore → Parser → ParseResult → Resolver → System
```

| 阶段 | 职责 |
|------|------|
| Reader | 从 /proc、/sys、命令输出等来源采集原始数据，存入 `RawStore` |
| Parser | 从 `RawStore` 中按域解析出结构化事实（`ParseResult`），各域独立，无跨域引用 |
| Resolver | 合并各域事实，解决来源冲突，计算进程可见性，组装最终的 `System` 对象 |

## 公共 API

```cpp
// 采集——一次调用，返回不可变对象
sysal::System sys = sysal::System::collect();

// 访问——直接成员，无需方法调用
const auto& cpu  = sys.info.cpu;
const auto& mem  = sys.info.memory;
const auto& gpus = sys.info.accelerators;

// 按需采集——位掩码组合
sysal::System partial = sysal::System::collect(
    sysal::Collect::Cpu | sysal::Collect::Accelerator | sysal::Collect::Raw
);

// 刷新——重新采集，替换内部状态
sys.refresh();
```

- 不需要全局 `init()`，后端初始化在 `collect()` 内部自动完成
- 失败时抛出 `SysalError`，部分失败记录到 `sys.warnings`
- `System` 构造后不可变，多线程 const 访问安全

## 与其他项目的关系

| 项目 | 回答的问题 |
|------|-----------|
| **sysal** | 这台机器有什么？当前进程能看到什么？ |
| **opal** | 应该选哪个算子？ |
| **opbl** | 这个算子跑得多快？ |

sysal 与 opal、opbl 保持独立。sysal 可能被 opal 使用，但不包含任何调度逻辑。
