# sysal

sysal 是一个 C++ 系统信息抽象库，用于服务器。

它采集服务器的硬件与软件信息，归一化为强类型数据结构，通过简洁的 API 交给调用方使用。sysal 只返回**事实**，不做**决策**。

## 项目定位

sysal 回答一个问题：**这台机器上有什么，当前进程能看到什么。**

它不回答：哪个设备最好、哪个算子该选、系统性能如何。那些是上层项目（opal、opbl）的职责。

sysal 是一个**库**，不是独立的系统巡检工具。

## 非目标

sysal 不是：

- 算子调度器
- 基准测试框架
- 性能评分系统
- 守护进程
- 监控系统
- Web 服务
- hwloc 的替代品

外部库（`NVML`、`ibverbs` 等）可作为后端使用，但 sysal 保持自己的公共数据模型，后端类型不泄漏到 API 中。

## 核心概念

sysal 遵循以下内部管线：

```
Reader → RawStore → Parser → ParseResult → Resolver → System
```

设计原则：**原始证据优先，类型化模型其次，决策永不。**

所有信息先以原始形态进入 `RawStore`，再由 Parser 解析为结构化类型，最后由 Resolver 组装为 `System` 对象。

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
- 适用于 MPI 多进程场景：每个进程独立创建自己的 `System` 对象

## 数据模型

`System` 对象持有以下公开成员：

| 成员 | 类型 | 说明 |
|------|------|------|
| `info` | `SystemInfo` | 系统信息本体（扁平结构） |
| `meta` | `SnapshotMeta` | 采集元数据（时间、版本、耗时） |
| `warnings` | `std::vector<std::string>` | 采集过程中的警告 |
| `raw` | `std::optional<RawStore>` | 可选的原始证据 |

`SystemInfo` 扁平包含各子系统：

- `Platform`：主机、OS、内核、架构、固件、虚拟化
- `Cpu`：packages、cores、逻辑 CPU、ISA 扩展
- `Memory`：总量、NUMA 内存分布
- `Accelerators`：GPU、NPU、FPGA 设备
- `Network`：网卡、链路状态、IP、PCI 地址
- `Storage`：块设备、容量、类型
- `Pci`：PCI 设备清单
- `SoftwareStack`：驱动、运行时、编译器、CUDA、ROCm、MPI、RDMA
- `ExecutionContext`：进程环境、cgroup、cpuset、容器、可见性索引

## 开发环境

| 工具 | 最低版本 | 说明 |
|------|----------|------|
| clang | 17 | C++23 编译器 |
| xmake | 2.8 | 构建系统 |
| lld | — | 链接器 |
| libc++ | — | C++ 标准库（随 clang 发布） |
| clang-format | — | 代码格式化（随 clang 发布） |
| clang-tidy | — | 静态分析（随 clang 发布） |
| nlohmann_json | — | JSON 序列化（通过 xmake `add_requires` 从 xrepo 管理） |

## 构建

```bash
xmake          # 构建（静态库 + 动态库 + 全部测试）
xmake -r       # 重新构建
```

构建产物：
- `libsysal.a` — 静态库（在 `build/.../static/` 子目录）
- `libsysal.so` — 动态库

`compile_commands.json` 通过 `xmake project -k compile_commands build` 生成（`utils/check.sh` 会自动调用）。

## 运行 sysal_info

```bash
xmake sysal_info    # 编译并运行 sysal_info，终端输出全部采集结果
```

## 测试

```bash
xmake run test_replay    # raw replay 测试
xmake run sysal_info      # 运行 sysal_info（输出被抑制，仅看退出码）
```

## CI

push 到 `main` 或 PR 时，GitHub Actions 自动运行 `utils/check.sh` 全量检查（clang-format + clang-tidy + build + tests）。

## 版本

当前版本 v0.0.3，版本号集中定义在 `include/sysal/version.hpp`。

## v0.0.3 范围

**实现**：公共 API（`System::collect` / `refresh` / `Collect` 位掩码）、全部数据模型、Linux 支持（procfs / sysfs / PCI）、nvidia-smi 命令输出采集、lspci 设备名解析、Storage HDD/SSD 检测、ISA 扩展（SSE/AVX/AVX-512/AES/FMA 等 17 项）、容器检测（Docker/Podman/LXC/K8s）、nlohmann/json 序列化、raw replay 测试。

**不实现**：性能评分、基准测试、算子选择、调度策略、守护进程、数据库存储、Web API、完整跨平台、拓扑信息（已有 hwloc）、NVML/ibverbs 库链接。

## v0.0.4 范围（即将发布）

**新增采集**：网络 IP 地址（`getifaddrs()`）、网络 PCI 地址（设备 sysfs 符号链接）、存储挂载点和文件系统类型（`df -Th`）、内存 DIMM 详情（`udevadm` + EDAC sysfs 双源策略）。

**优化**：syscall 优化（`uname()` 替代 `/proc/version`，`gethostname()` 替代 `/proc/sys/kernel/hostname`）、nlohmann/json 从 vendor 迁移至 xrepo 管理、`sysal.hpp` 移至 `include/sysal/` 顶层。

**不实现**：与 v0.0.3 相同（性能评分、基准测试、算子选择、调度策略、守护进程、数据库存储、Web API、完整跨平台、拓扑信息、NVML/ibverbs 库链接）。

## 与其他项目的关系

| 项目 | 回答的问题 |
|------|-----------|
| **sysal** | 这台机器有什么？当前进程能看到什么？ |
| **opal** | 应该选哪个算子？ |
| **opbl** | 这个算子跑得多快？ |

sysal 与 opal、opbl 保持独立。

## License

Apache License 2.0
