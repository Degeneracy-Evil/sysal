# 内部管线与模块结构

## 管线

```txt
Reader → RawStore → Parser → ParseResult → Resolver → System
```

- **Reader**：将原始信息收集到 `RawStore`。数据来源包括 procfs（`/proc`）、
  sysfs（`/sys`）、POSIX syscall（`getifaddrs()`、`uname()`、`gethostname()`、
  `readlink()`）、命令执行（`lspci`、`nvidia-smi`、`df -Th`、`udevadm`）、
  以及 EDAC sysfs（`/sys/devices/system/edac`）。
- **Parser**：将 `RawStore` 转换为按域划分的 `ParseResult`（无跨域引用，直接使用公共类型）。
- **Resolver**：合并 `ParseResult`，解决冲突，计算可见性，组装 `System` 对象。

## ParseResult（内部契约）

```cpp
namespace sysal::detail
{

struct ParseResult
{
    std::optional<Platform>          platform;
    std::optional<Cpu>               cpu;
    std::optional<Memory>            memory;
    std::optional<Pci>               pci;
    std::optional<Network>           network;
    std::optional<Accelerators>      accelerators;
    std::optional<Storage>           storage;
    std::optional<SoftwareStack>     software;
    std::optional<ExecutionContext>  execution;
    std::optional<Sensors>           sensors;
};

}  // namespace sysal::detail
```

`ParseResult` 位于 `src/parser/parse_result.hpp`（内部，不在 `include/sysal/` 中）。
每个字段都是 `optional` —— 该域可能失败或未被请求。
各字段直接使用公共 API 中定义的类型，不再使用私有 `*Facts` 结构。

## 源码职责

| 目录 | 职责 |
| --- | --- |
| include/sysal/core | System、Collect、错误契约 |
| include/sysal/model、types | 公共领域模型、强类型 ID 与单位 |
| src/reader | 只读采集与后端适配，生成 RawStore |
| src/parser | 各域解析与来源归一化 |
| src/resolver | 跨域关联、可见性与派生健康证据 |
| src/pipeline | 域分派、采集状态、警告和快照元数据 |
| src/serialization | 各领域 JSON 转换与共享值验证；serialize.cpp 组装顶层快照 |
| src/api、model | 公共入口与模型查询实现 |

存储控制器与 PCI/NUMA 的关联由 resolver/storage_connections 处理，
parser/storage_connections 仅解析原始协议库存。领域序列化函数是内部接口；
公共序列化入口仍位于 include/sysal/serialization/serialization.hpp。

完整字段契约以公共头文件为准，避免在设计文档复制全部文件列表。
