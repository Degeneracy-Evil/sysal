# 开发记录

### 2026-06-20 从 base_project 迁移脚手架

- **变更类型**: chore / build / docs
- **涉及文件**: xmake.lua, .clang-format, .clang-tidy, .clangd, .gitignore, .githooks/pre-commit, utils/check.sh, AGENTS.md, docs/devlog.md, include/sysal/sysal.hpp, src/sysal.cpp
- **变更内容**: 从 base_project 迁移工程基础设施模板，包括 xmake 构建配置（C++23/clang/libc++/lld）、clang-format 格式化规则、clang-tidy 静态分析配置、clangd IDE 集成、git hooks（pre-commit 转发到 check.sh）、统一质量检查脚本（hook/全量双模式）。适配为 sysal 静态库目标，创建 include/sysal/ 和 src/ 目录结构及占位文件。check.sh 文件收集改为 find 递归以适配子目录布局。
- **原因**: sysal 项目起步，需要一套完整的 C++ 工程基础设施
- **验证**: `xmake build` 通过，`utils/check.sh` 全量检查通过

### 2026-06-20 同步 base_project 模板更新

- **变更类型**: chore / build / docs / ci
- **涉及文件**: .gitattributes, .clang-tidy, xmake.lua, .github/workflows/ci.yml, tests/.gitkeep, AGENTS.md, README.md, docs/devlog.md
- **变更内容**:
  1. 创建 `.gitattributes`，统一行尾为 LF
  2. `.clang-tidy` 设置 `WarningsAsErrors: '*'`，静态分析零容忍
  3. `xmake.lua` 添加 `after_build` 钩子，构建后自动生成 `compile_commands.json`（手动构造 JSON，避免 xmake 递归调用死锁）
  4. 创建 `.github/workflows/ci.yml`，push/PR 时自动安装工具链并运行 `utils/check.sh`
  5. 添加 `tests/.gitkeep`，确保空目录被 git 跟踪
  6. AGENTS.md / README.md 更新约定描述：前置条件表格、CI 说明、自动 compile_commands、clang-tidy 零容忍、.gitattributes
- **原因**: 与 base_project 模板保持同步，提升开箱即用性和 CI 就绪度
- **验证**: `xmake -r` 后 `compile_commands.json` 自动生成，`utils/check.sh` 4/4 通过

### 2026-06-20 撰写设计修订提案

- **变更类型**: docs
- **涉及文件**: docs/design_proposals.md, docs/devlog.md
- **变更内容**: 针对 design_document.md 初版识别出的 14 项架构问题，逐项撰写详细设计提案，涵盖公共 API 统一（CollectSpec builder）、CPU 层级关系（parent ID）、可见性模型统一（per-device flag）、Topology/Pci 职责划分、SystemSnapshot 元数据、RawStore 多记录、AcceleratorSubsystem 定义、Pipeline 中间表示（ParsedFacts）、冲突解决策略（source trust order）、序列化、线程安全、缓存预留、跨平台扩展、测试策略（raw replay）。每项含问题陈述、C++ 类型定义、理由、对原设计影响。未修改原有 design_document.md。
- **原因**: design_document.md 初版存在命名不自洽、层级关系丢失、冲突策略缺失等问题，需在实现前补齐设计
- **验证**: 文档审阅（无代码变更）

### 2026-06-20 合并设计提案为唯一设计文档

- **变更类型**: docs
- **涉及文件**: docs/design_document.md, docs/design_proposals.md (删除), README.md, docs/devlog.md
- **变更内容**: 将 14 项设计提案合并进 design_document.md，形成唯一完整设计文档（23 节）。补齐了原文档缺失的类型定义（CollectStatus、NetworkSubsystem、StorageSubsystem、NumaNode、NumaMemoryInfo、Architecture、IsaExtension、StrongId、Expected 等）。修正了 API 风格不一致（README 用方法调用 `snapshot.resources()` 改为成员访问 `snapshot.resources`）。新增章节：冲突解决策略（§12）、线程安全（§16）、序列化（§17）、测试策略 raw replay（§18）、未来扩展（§19）。删除 docs/design_proposals.md。
- **原因**: 消除多份文档间的混乱，确保初开发阶段只有一份自洽的完整设计文档
- **验证**: 文档审阅，无代码变更

### 2026-06-20 拆分设计文档为按层级组织的小文件

- **变更类型**: docs / refactor
- **涉及文件**: docs/design_document.md (删除), docs/design/index.md, docs/design/overview.md, docs/design/public_api.md, docs/design/data_model/*.md (8 files), docs/design/architecture/*.md (2 files), docs/design/rules/*.md (3 files), docs/design/testing/*.md (2 files), docs/design/roadmap.md, docs/devlog.md
- **变更内容**: 将单文件 design_document.md (23 节, 1144 行) 拆分为 19 个独立小文件，按架构层级组织在 docs/design/ 下：概览层、公共 API 层、数据模型层 (8 文件)、内部架构层 (2 文件)、设计规则层 (3 文件)、测试层 (2 文件)、路线图层。创建 index.md 作为索引目录，含目录结构、文档索引表、推荐阅读顺序。删除原 design_document.md。
- **原因**: 单文件过大难以导航和维护，拆分后每个文件职能单一、便于独立查阅和更新
- **验证**: 文档审阅，无代码变更

### 2026-06-20 同步 base_project CI workflow 更新

- **变更类型**: ci
- **涉及文件**: .github/workflows/ci.yml, docs/devlog.md
- **变更内容**: CI workflow 更新：ubuntu-latest → ubuntu-24.04；安装包加 build-essential / libc++abi-dev，去 libunwind-dev；xmake 安装改用 GitHub Action xmake-io/github-action-setup-xmake@v1；新增 Configure (xmake f -c -y --toolchain=clang) 和 Build (xmake build) 步骤
- **原因**: 与 base_project 模板保持同步
- **验证**: 文件比对一致

### 2026-06-21 实现 v0.0.1 内部管线与公共 API

- **变更类型**: src / build / fix
- **涉及文件**: xmake.lua, .clang-tidy, include/sysal/collect.hpp, src/sysal.cpp, src/parser/parsed_facts.hpp, src/parser/parse_utils.hpp, src/parser/cpu_parser.hpp, src/parser/cpu_parser.cpp, src/parser/memory_parser.hpp, src/parser/memory_parser.cpp, src/parser/platform_parser.hpp, src/parser/platform_parser.cpp, src/parser/network_parser.hpp, src/parser/network_parser.cpp, src/parser/pci_parser.hpp, src/parser/pci_parser.cpp, src/reader/linux/file_utils.hpp, src/reader/linux/procfs_reader.hpp, src/reader/linux/procfs_reader.cpp, src/reader/linux/sysfs_reader.hpp, src/reader/linux/sysfs_reader.cpp, src/resolver/resolver.hpp, src/resolver/resolver.cpp, src/public_api/collect.cpp, tests/test_collect.cpp, docs/devlog.md
- **变更内容**: 实现完整 Reader→RawStore→Parser→ParsedFacts→Resolver→SystemSnapshot 管线。新增公共 API 声明头 include/sysal/collect.hpp（collect / collect_or_throw）。新增内部 ParsedFacts 结构（CpuFacts / MemoryFacts / PciFacts / NetworkFacts / PlatformFacts）。新增 Linux procfs reader（/proc/cpuinfo、/proc/meminfo、/proc/version、/etc/os-release、uname syscall）和 sysfs reader（CPU 拓扑、NUMA、网络接口、PCI 设备）。新增 5 个 parser（CPU/内存/平台/网络/PCI），将原始数据解析为 ParsedFacts。新增 resolver 将 ParsedFacts 组装为 SystemSnapshot，填充 SnapshotMeta、ExecutionContextInfo、可见性标志。新增 collect.cpp 实现 collect() 和 collect_or_throw() 公共 API。xmake.lua 添加 add_includedirs("src") 和 test_collect 二进制目标。.clang-tidy 添加 -bugprone-unchecked-optional-access 抑制（expected.hpp 中 operator*/error 为有意设计，且该头文件不可修改）。
- **原因**: sysal v0.0.1 核心功能实现，需要可工作的 collect() / collect_or_throw() 从 Linux 系统收集 CPU、内存、平台、网络信息
- **验证**: `xmake -r` 构建成功；`clang-format --dry-run --Werror` 全部通过；`clang-tidy` 零警告；`xmake run test_collect` 输出合理的 CPU 包数、逻辑 CPU 数、内存总量、主机名、OS 版本、内核版本信息

### 2026-06-21 架构简化 — 消除 ParsedFacts 类型重复 + collect_spec inline + resolver move

- **变更类型**: refactor / src
- **涉及文件**: src/parser/parsed_facts.hpp, src/parser/cpu_parser.hpp, src/parser/cpu_parser.cpp, src/parser/memory_parser.hpp, src/parser/memory_parser.cpp, src/parser/platform_parser.hpp, src/parser/platform_parser.cpp, src/parser/network_parser.hpp, src/parser/network_parser.cpp, src/parser/pci_parser.hpp, src/parser/pci_parser.cpp, src/resolver/resolver.hpp, src/resolver/resolver.cpp, src/public_api/collect.cpp, include/sysal/collect_spec.hpp, src/collect_spec.cpp (删除), docs/devlog.md
- **变更内容**:
  1. `parsed_facts.hpp` 删除 CpuFacts/MemoryFacts/PciFacts/NetworkFacts/PlatformFacts 5 个重复类型定义，改用公共类型 CpuSubsystem/MemorySubsystem/PciSubsystem/NetworkSubsystem/PlatformInfo
  2. 5 个 parser 头文件和实现文件的返回类型同步更新
  3. `resolver.cpp` 删除 5 个 fill_* 逐字段拷贝函数，改为 `std::move(*facts.xxx)` 直接赋值；resolve 签名改为右值引用 `ParsedFacts&&`
  4. `collect_spec.hpp` 全部 22 个 getter/setter + 3 个工厂方法 inline 到头文件；删除 `src/collect_spec.cpp`（135 行）
  5. `collect.cpp` 调用 resolve 时传 `std::move(facts)`
- **原因**: ParsedFacts 重复定义了与公共类型 1:1 映射的内部类型，resolver 逐字段拷贝是纯冗余；collect_spec 22 个 boilerplate 函数不应有 .cpp 文件
- **验证**: `xmake -r` 构建成功；`clang-format` 通过；`xmake run test_collect` 输出不变

### 2026-06-21 消除代码冗余 — 提取共享抽象

- **变更类型**: refactor / src
- **涉及文件**: src/detail/algorithm.hpp, src/resource_info.cpp, src/reader/linux/procfs_reader.cpp, src/reader/linux/sysfs_reader.cpp, src/parser/parse_utils.hpp, src/parser/cpu_parser.cpp, src/parser/platform_parser.cpp, src/parser/network_parser.cpp, src/parser/pci_parser.cpp, src/raw_store.cpp, include/sysal/value_types.hpp, include/sysal/units.hpp, include/sysal/diagnostics.hpp, tests/test_collect.cpp, docs/devlog.md
- **变更内容**:
  1. 新增 `src/detail/algorithm.hpp`，提供 `find_if` / `filter_if` 模板；`resource_info.cpp` 12 个线性搜索方法全部改用模板
  2. `procfs_reader.cpp` 提取 `read_proc_file` 辅助函数，删除 4 个近重复函数
  3. `sysfs_reader.cpp` 提取 `read_sysfs_dir` 目录遍历辅助函数，4 个 sysfs 读取函数统一改用该辅助
  4. `parse_utils.hpp` 新增 `arch_from_machine` 和 `extract_prefix_keys` 共享工具；`cpu_parser.cpp` 删除本地 `determine_arch` 中的重复映射逻辑，`platform_parser.cpp` 删除本地 `arch_from_machine`；`network_parser.cpp` 和 `pci_parser.cpp` 删除各自的 `extract_interface_names` / `extract_device_addresses`，统一调用 `extract_prefix_keys`
  5. `value_types.hpp` 用 `NamedString<Tag>` 模板替换 5 个重复字符串包装结构体
  6. `units.hpp` 用 `ScalarUnit<Tag>` 模板替换 3 个重复标量单位结构体；`test_collect.cpp` 中 `.bytes` 访问改为 `.value`
  7. `raw_store.cpp` 的 `count` 方法改用 `std::ranges::count_if`
  8. `diagnostics.hpp` 移除不必要的 `#include "sysal/raw_store.hpp"`，改为直接包含 `<vector>`
- **原因**: 消除 12+4+4+2+5+3 个近重复代码模式，提升可维护性；统一抽象后新增类型只需复用模板
- **验证**: `utils/check.sh` 全部 4 项检查通过（clang-format / clang-tidy / build / tests）；`xmake run test_collect` 输出与重构前一致

### 2026-06-21 实现 execution_parser 执行上下文解析器

- **变更类型**: src
- **涉及文件**: src/parser/execution_parser.cpp, docs/devlog.md
- **变更内容**:
  1. 进程信息：`getpid/getuid/getgid/geteuid/getegid` 填充 `ProcessInfo`，`is_root = (geteuid == 0)`
  2. Cgroup 解析：从 `RawSource::ProcSelfCgroup` 读取，`0::` 前缀检测 v2，否则按 v1 取首行路径
  3. Cpuset 解析：从 `RawSource::ProcSelfStatus` 读取 `Cpus_allowed_list` / `Mems_allowed_list`，`parse_id_list` 支持 `N` 和 `N-M` 范围语法，填充 `LogicalCpuId` / `NumaNodeId`；cpus 非空时 `is_restricted = true`
  4. 容器检测：`/.dockerenv` 存在 → Docker；`/proc/1/cgroup` 内容匹配 docker/podman/lxc/kube 模式；环境变量 `container` → Podman、`KUBERNETES_SERVICE_HOST` → Kubernetes；无匹配则 `container = nullopt`
  5. 环境变量：`getenv` 读取 `CUDA_VISIBLE_DEVICES` / `HIP_VISIBLE_DEVICES` / `ONEAPI_DEVICE_SELECTOR` / `OMP_NUM_THREADS` / `MLU_VISIBLE_DEVICES`，仅收录已设置变量
  6. 权限能力：从 `ProcSelfStatus` 读取 `CapEff` 十六进制串存入 `PermissionInfo::capabilities`
- **原因**: 完成 v0.0.1 执行上下文收集功能，覆盖进程/cgroup/cpuset/容器/环境/权限六个维度
- **验证**: `xmake -r` 构建通过（-Wall -Wextra -Werror 零 warning）；LSP diagnostics 无报错

### 2026-06-21 实现 accelerator_parser 和 software_parser

- **变更类型**: src
- **涉及文件**: src/parser/accelerator_parser.cpp, src/parser/software_parser.cpp, docs/devlog.md
- **变更内容**:
  1. `accelerator_parser.cpp`：解析 `RawSource::NvidiaSmi` 中 `path_or_command` 以 "nvidia-smi" 开头的成功记录，按 CSV 格式（index, name, memory.total, pci.bus_id, driver_version）逐行解析。每行 split by comma + trim，构造 `AcceleratorDevice`（kind=Gpu, vendor=NVIDIA, memory 由 MiB 转 bytes, pci_address 用 `parse_pci_address`，visible=true）。无数据或命令失败返回 `nullopt`
  2. `software_parser.cpp`：从三处来源提取 NVIDIA 软件栈信息：(a) `/proc/driver/nvidia/version` 的 NVRM version 行提取驱动版本（空格分隔的 version-like token）；(b) `nvcc --version` 的 release 行提取 CUDA runtime 版本；(c) nvidia-smi CSV 末列提取驱动版本并统计 device_count。填充 `CudaInfo`、`DriverInfo{name=nvidia, loaded=true}`、`RuntimeInfo{name=cuda}`。无 NVIDIA 软件返回 `nullopt`
- **原因**: 完成 v0.0.1 GPU 加速器和软件栈解析功能
- **验证**: `xmake -r` 构建通过；`clang-format --dry-run --Werror` 两文件通过；`clang-tidy` 零警告；LSP diagnostics 无报错

### 2026-06-21 扩展 network_parser 并实现 storage_parser

- **变更类型**: src
- **涉及文件**: src/parser/network_parser.cpp, src/parser/storage_parser.cpp, docs/devlog.md
- **变更内容**:
  1. `network_parser.cpp`：在原有 name/state/speed/MAC 解析基础上新增两项。(a) IP 地址：通过 `getifaddrs()` 系统调用直接获取每个接口的 AF_INET/AF_INET6 地址，用 `inet_ntop` 转为字符串存入 `NetworkInterface::addresses`，避免解析复杂的 `/proc/net/fib_trie`。(b) PCI 地址：从 `path_map` 查找 `/sys/class/net/<iface>/device` 记录（sysfs_reader 已读取 device 符号链接的 filename），用 `parse_pci_address()` 解析后存入 `NetworkInterface::pci_address`。`diag` 参数从忽略改为使用，接口列表为空时发 warning
  2. `storage_parser.cpp`：从 stub 改为完整实现。用 `build_path_map` + `extract_prefix_keys`（前缀 `/sys/block/`）提取块设备名列表。对每个设备：从 `size` 记录解析扇区数 × 512 得 `MemorySize` capacity；从 `device/model` 解析型号覆盖 DeviceName；从 `device` 符号链接解析 PCI 地址；按名称前缀分类（nvme→Nvme, sd→Sata, else→Other）；构造 `StorageDevice{StorageId{index}, ...}`。无数据返回 `nullopt`
- **原因**: 完成 v0.0.1 网络接口 IP/PCI 地址收集和块设备存储解析功能
- **验证**: `xmake -r` 构建通过（-Wall -Wextra -Werror 零 warning）；`clang-format -i` 两文件通过；`clang-tidy` 零警告；LSP diagnostics 无报错

### 2026-06-21 实现 topology 解析、NUMA 内存解析与 hwloc 后端

- **变更类型**: src
- **涉及文件**: src/parser/topology_parser.cpp, src/parser/memory_parser.cpp, src/backend/hwloc_backend.hpp, src/backend/hwloc_backend.cpp, docs/devlog.md
- **变更内容**:
  1. `topology_parser.cpp`：从 stub 改为完整实现。优先尝试 hwloc 后端（`parse_topology_hwloc`），成功则直接返回；否则回退到 sysfs 解析。sysfs 路径用 `build_path_map` + `extract_prefix_keys`（前缀 `/sys/devices/system/node/`）提取 NUMA 节点列表，从 `node<N>/meminfo` 解析 `MemTotal` 行（KB→bytes）构造 `NumaRelation{node_id, packages=空, local_memory}`；用前缀 `/sys/bus/pci/devices/` 提取 PCI 设备列表，从 `numa_node` 文件解析节点号（跳过负值 -1），构造 `DeviceLocality{pci_address, nearest_numa_node}`。无数据返回 `nullopt`
  2. `memory_parser.cpp`：在 `/proc/meminfo` 解析基础上新增 NUMA 内存解析。用 `build_path_map` + `extract_prefix_keys`（前缀 `/sys/devices/system/node/`）遍历各 NUMA 节点，从 `node<N>/meminfo` 解析 `MemTotal`（→total）和 `MemFree`（→available），构造 `NumaMemoryInfo` 填入 `MemorySubsystem::numa_memory` 向量
  3. `hwloc_backend.hpp`：声明 `parse_topology_hwloc(Diagnostics&) -> optional<TopologyInfo>`
  4. `hwloc_backend.cpp`：用 `#ifdef SYSAL_HAVE_HWLOC` 条件编译包裹 hwloc API 调用。启用 `HWLOC_OBJ_PCI_DEVICE` 类型过滤器后 `hwloc_topology_load`；遍历 `HWLOC_OBJ_NUMANODE` 层读取 `obj->attr->numanode.local_memory` 构造 `NumaRelation`；遍历 `HWLOC_OBJ_PCI_DEVICE` 层读取 `obj->attr->pcidev.{domain,bus,dev,func}` 构造 `PciAddress`，沿 parent 链向上查找 memory_first_child 中的 NUMA 节点作为 `nearest_numa_node` 构造 `DeviceLocality`。未定义 `SYSAL_HAVE_HWLOC` 时直接返回 `nullopt`
- **原因**: 完成 v0.0.1 拓扑信息收集（NUMA 关系、PCI 设备局部性）和 NUMA 内存分布解析，支持 hwloc 可选后端
- **验证**: `xmake -r` 构建通过（-Wall -Wextra -Werror 零 warning）；`clang-format --dry-run --Werror` 四文件通过；`clang-tidy --warnings-as-errors='*'` 零用户代码警告

### 2026-06-21 实现 JSON 序列化与 raw replay 测试基础设施

- **变更类型**: src / build / docs
- **涉及文件**: include/sysal/serialization.hpp, src/serialization.cpp, include/sysal/test/replay.hpp, src/test/replay.cpp, src/detail/json.hpp, tests/test_replay.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. `include/sysal/serialization.hpp`：声明 `SerializationOptions`（pretty_print / include_raw / include_meta）和公共 API `to_json` / `from_json`
  2. `src/detail/json.hpp`：内部 JSON 工具库（header-only）。包含 `escape_string`、`JsonObj` / `JsonArr` 构建器、`JsonVal` DOM 节点、`JsonParser` 递归下降解析器、`parse_json`、`raw_store_to_json` / `raw_store_from_json`、`time_point_to_ms` / `ms_to_time_point`。手写实现，无外部依赖
  3. `src/serialization.cpp`：`to_json` 序列化 SystemSnapshot 全部字段（meta / platform / resources / software / execution / diagnostics / raw），枚举序列化为整数、StrongId 序列化为 `.value()`、NamedString 序列化为 `.value` 字符串、ScalarUnit 序列化为 `.value` 整数、optional 字段省略、time_point 序列化为 epoch 毫秒。`from_json` 解析 JSON DOM 并提取 meta 和 raw（v0.0.1 基本反序列化）
  4. `include/sysal/test/replay.hpp`：声明 `load_raw_store` / `collect_from_raw` / `save_raw_store`
  5. `src/test/replay.cpp`：`save_raw_store` 将 RawStore 序列化为 JSON 文件；`load_raw_store` 解析 JSON 文件为 RawStore；`collect_from_raw` 调用 `sysal::detail::run_pipeline(raw, spec, now)`
  6. `tests/test_replay.cpp`：capture → save → load → replay → verify 流程，验证 CPU 数量、内存总量、NUMA 节点、网络接口、PCI 设备、存储设备、加速器数量一致
  7. `xmake.lua`：新增 `test_replay` 目标
- **原因**: 实现 design_document §17 序列化和 §18 raw replay 测试策略，为回归测试提供基础设施
- **验证**: `xmake -r` 构建通过；`xmake run test_replay` 全部 9 项检查 PASS；`utils/check.sh` 4/4 通过（clang-format + clang-tidy + build + tests）

### 2026-06-21 v0.0.1 全功能完成：基础设施 + pipeline 重构

- **变更类型**: src / build / refactor
- **涉及文件**: xmake.lua, include/sysal/enums.hpp, src/parser/parsed_facts.hpp, src/parser/{topology,execution,accelerator,software,storage}_parser.{hpp,cpp}, src/reader/linux/{procfs,sysfs}_reader.cpp, src/reader/linux/file_utils.hpp, src/resolver/resolver.cpp, src/public_api/collect.cpp, src/detail/pipeline.{hpp,cpp}, src/backend/hwloc_backend.{hpp,cpp}, tests/test_collect.cpp
- **变更内容**:
  1. 基础设施：enums 新增 SysfsBlock/ProcSelfCgroup/ProcSelfStatus/ProcNetInet；ParsedFacts 新增 accelerator/storage/topology/software/execution 字段；xmake.lua 添加 hwloc pkg-config 探测（SYSAL_HAVE_HWLOC）
  2. Reader 扩展：procfs_reader 新增 cgroup/status/fib_trie/nvidia-smi/nvcc 读取；sysfs_reader 新增 NUMA meminfo/PCI numa_node/network device symlink/block device 读取；file_utils 新增 read_command (popen)
  3. Pipeline 重构：提取 src/detail/pipeline.{hpp,cpp} 共享 parse+resolve 逻辑，collect.cpp 和 replay.cpp 共用
  4. Resolver 增强：处理 accelerator/storage/topology/software/execution；cpuset 驱动 CPU 可见性；预计算 visible_*_ids 索引
  5. test_collect 升级为 CollectSpec::full() 全功能验证
- **原因**: 完成 v0.0.1 roadmap 所有目标——CPU/Memory/NUMA/Accelerator/Network/PCI/Storage/Topology/Software/Execution/Serialization/RawReplay
- **验证**: `utils/check.sh` 4/4 通过；`xmake run test_collect` 输出 2 CPU packages, 52 logical CPUs, 405GB memory, 2x A100 80GB GPU, 304 PCI devices, 13 storage devices, 2 NUMA relations, 10 network interfaces; `xmake run test_replay` 9/9 PASS

### 2026-06-21 代码质量清理：去冗余 + Bug 修复 + 错误处理补全

- **变更类型**: refactor / fix / src
- **涉及文件**: src/parser/parse_utils.hpp, src/parser/memory_parser.cpp, src/parser/topology_parser.cpp, src/resolver/resolver.cpp, src/parser/execution_parser.cpp, src/parser/network_parser.cpp, src/parser/platform_parser.cpp, src/parser/pci_parser.cpp, src/parser/storage_parser.cpp, src/parser/software_parser.cpp, src/reader/linux/procfs_reader.cpp, src/sysal.cpp(删除)
- **变更内容**:
  1. **去冗余**：`extract_kb`/`node_id_from_key`/`kNodePrefix` 从 memory_parser + topology_parser 合并到 parse_utils.hpp；删除 src/sysal.cpp 空文件；删除 procfs_reader 中未使用的 /proc/net/fib_trie 读取
  2. **Bug 修复**：删除 resolver 中 `fill_execution_context` 死代码（与 execution_parser 重复，覆盖已填充的值）；修复 `is_restricted` 启发式（原逻辑：cpus 非空即 restricted → 所有系统都被标记为 restricted；新逻辑：cpuset 是 CPU 总数的真子集时才 restricted）；修复 resolver 可见性 O(n×m) → O(n+m) 使用 `unordered_set<LogicalCpuId>` 查找；修复 network_parser O(n²) getifaddrs（原：每个接口单独调用 getifaddrs 遍历全部；新：调用一次构建 name→addresses 映射）
  3. **错误处理补全**：5 个忽略 `diag` 参数的 parser 改为使用 `add_warning`：platform_parser（缺少 uname 架构信息时告警）、pci_parser（sysfs 记录存在但无设备时告警）、storage_parser（同上）、software_parser（NVIDIA 数据收集失败时告警）、execution_parser（无法确定 cgroup 路径时告警）
- **原因**: 代码质量审计发现 7 项冗余 + 4 项 Bug + 5 个 parser 缺失错误处理
- **验证**: `utils/check.sh` 4/4 通过；`xmake run test_replay` 9/9 PASS
