# 数据源选择原则

> Data Source Selection Guideline

## 核心原则

```
优先级：syscall > 文件读取 > 命令执行
```

sysal 在采集系统信息时，按以下优先级选择数据源：

1. **POSIX syscall** — 最优先
2. **文件读取**（/proc、/sys）— 次优先
3. **命令执行**（外部工具）— 最后选择

## 理由

### Syscall 最可靠

- **POSIX 标准**：syscall 是操作系统提供的标准接口，接口稳定，跨平台兼容性好。
- **无外部依赖**：不需要特定文件系统挂载，不需要外部工具安装。
- **容器中也可用**：即使容器内 /proc 和 /sys 被部分屏蔽，syscall 通常仍可正常工作。
- **无解析误差**：syscall 直接返回结构化数据，无需解析文本输出。

### 文件读取次之

- /proc 和 /sys 是 Linux 标准接口，几乎所有 Linux 发行版都支持。
- 但格式可能因内核版本变化，需要健壮的解析逻辑。
- 容器环境下部分文件可能不可见或内容被隔离。

### 命令执行最后

- 依赖外部工具的存在性（如 nvidia-smi、lspci、df、udevadm）。
- 有子进程创建和 IPC 开销。
- 输出格式可能因工具版本变化，且不同语言环境（locale）可能影响输出。
- 在最小化容器中可能完全不可用。

## 决策表

### 使用 syscall 的数据源

| 数据 | syscall | 原因 |
|------|---------|------|
| 网络接口 IP 地址 | `getifaddrs()` | POSIX 标准，返回结构化地址列表，无解析开销 |
| CPU 架构 / 内核版本 | `uname()` | POSIX 标准，单次调用获取全部信息，替代多次文件读取 |
| 主机名 | `gethostname()` | POSIX 标准，比读取 `/proc/sys/kernel/hostname` 更可靠 |
| PCI 符号链接解析 | `readlink()` | 标准 C 库函数，解析 sysfs 符号链接获取 PCI 地址 |

### 使用文件读取的数据源

| 数据 | 文件 | 原因 |
|------|------|------|
| CPU 详细信息 | `/proc/cpuinfo` | 无 syscall 可替代，提供完整的 CPU 拓扑和特性标志 |
| 内存总量 / 可用量 | `/proc/meminfo` | 比 `sysconf(_SC_PHYS_PAGES)` 提供更丰富的信息（MemAvailable 等） |
| cgroup 信息 | `/proc/self/cgroup`、`/proc/1/cgroup` | 无 syscall 替代 |
| NUMA 拓扑 | `/sys/devices/system/node/*` | 无 syscall 替代 |
| 网络接口信息 | `/sys/class/net/*` | 无 syscall 替代 |
| 块设备信息 | `/sys/block/*` | 无 syscall 替代 |
| DMI 信息 | `/sys/class/dmi/id/*` | 无 syscall 替代 |
| PCI 设备 | `/sys/bus/pci/devices/*` | 无 syscall 替代 |
| OS 发行版 | `/etc/os-release` | 标准来源，无 syscall 替代 |
| 内存 DIMM（备） | `/sys/devices/system/edac/*` | udevadm 不可用时的回退方案 |
| 进程状态 | `/proc/self/status` | 提供 CapEff、cpus_allowed_list 等多字段，优于单独 syscall |

### 使用命令执行的数据源

| 数据 | 命令 | 原因 |
|------|------|------|
| GPU 信息 | `nvidia-smi` | 无 syscall 替代，NVML 库为可选后端 |
| CUDA 版本 | `nvcc --version` | 无 syscall 替代 |
| PCI 设备名 | `lspci` | 补充 sysfs 中缺失的设备名称和 vendor 信息 |
| 存储挂载信息 | `df -Th` | 比 `statvfs()` 更简单，一次性获取所有挂载点和文件系统类型 |
| 硬件数据库 | `udevadm info -e` | 无 syscall 替代，提供最完整的 DIMM 信息 |

## 例外规则

当 syscall 只能获取部分数据而文件可以获取全部时，使用文件读取。

**示例**：`/proc/self/status`

`getpid()`、`getppid()`、`getuid()` 等单独 syscall 可以获取进程的部分信息，
但 `/proc/self/status` 一次性提供了更多字段（CapEff、cpus_allowed_list、
voluntary_ctxt_switches 等），因此保留文件读取方式。

## 新增数据源检查清单

在 sysal 中新增数据源时，按以下顺序检查：

1. **是否有 POSIX syscall 可以获取此数据？**
   - 查阅 POSIX 标准或 Linux man pages。
   - 如果有，优先使用 syscall。

2. **syscall 是否能获取全部所需字段？**
   - 如果 syscall 返回的数据不完整，评估是否仍需文件读取作为补充。

3. **如果 syscall 只能获取部分字段，文件读取是否仍需要？**
   - 参考例外规则：当文件读取能一次性提供更多字段时，保留文件读取。

4. **如果没有 syscall，是否可以用文件读取替代命令执行？**
   - 检查 /proc 或 /sys 中是否有对应接口。
   - 文件读取优先于命令执行。

5. **命令是否在目标系统上一定存在？**
   - 检查命令的默认安装情况。
   - 考虑容器环境下的可用性。
   - 如果命令可能不存在，必须有降级策略（optional 字段或回退方案）。
