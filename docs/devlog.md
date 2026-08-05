# 开发记录

### 2026-08-05 软件栈：MPI 填充

- **变更类型**: src / tests
- **涉及文件**: include/sysal/types/enums.hpp, src/reader/linux/procfs.cpp, src/parser/software.cpp, examples/sysal_info.cpp, tests/unit/test_parse_software.cpp, docs/devlog.md
- **变更内容**:
  1. enums.hpp 追加 `MpiVersion`（mpirun --version）、`MpiPath`（command -v）两枚 RawSource，追加在末尾保持枚举值稳定
  2. procfs.cpp Software 域：探测 `mpirun --version` 与 `command -v mpirun`；缺失时静默记 Failed
  3. software.cpp：新增 `extract_mpi_info`（从首行括号提取实现名、末 token 提取版本，兼容 OpenMPI/MPICH/MVAPICH2）；已就绪时填充 `stack.mpi`
  4. 空栈 nullopt 判定与无数据告警条件补充 MPI 维度
- **原因**: 软件栈逐个子域的第二个（MPI）
- **验证**: `xmake` 构建通过；18/18 测试套件通过（software 78）；`sysal_info` 实测 Open MPI 4.1.9a1；clang-tidy `--warnings-as-errors` 无告警

### 2026-08-05 软件栈：编译器填充

- **变更类型**: src / tests
- **涉及文件**: include/sysal/types/enums.hpp, src/reader/linux/procfs.cpp, src/parser/software.cpp, examples/sysal_info.cpp, tests/unit/test_parse_software.cpp, tests/unit/test_parse_storage.cpp, docs/devlog.md
- **变更内容**:
  1. enums.hpp 追加三枚 RawSource：`CompilerVersion`（--version 输出）、`CompilerPath`（command -v）、`CompilerTarget`（-dumpmachine），追加在末尾保持枚举值稳定
  2. procfs.cpp Software 域：遍历 gcc/g++/clang/clang++/gfortran 逐个探测三条命令；缺失的命令静默记 Failed
  3. software.cpp：新增 `extract_compiler_version`（从 --version 首行提取 X.Y.Z 纯数字 token，兼容 gcc/clang/gfortran 格式）、`first_success`（按来源+命令取首条 Success 记录）；填充 `stack.compilers`，缺失编译器不产生 warning
  4. 空栈 nullopt 判定修正：仅在完全不采集到任何软件数据时告警，数据存在但解析失败属静默场景
- **原因**: 软件栈逐个填充分阶段的第一项（编译器）；遵循"缺失工具不刷 warning"的静默设计原则
- **验证**: `xmake` 构建通过；18/18 测试套件通过；`sysal_info` 实测识别本机 5 个编译器（gcc/g++/clang/clang++/gfortran）；clang-tidy `--warnings-as-errors` 无告警
- **其他**: 清理了 Aug 03-04 遗留的孤儿 docker/xmake 构建进程（持有 project.lock 导致本地 xmake 增量构建卡死）；修正 storage 测试 2.5 的设备字典序断言

### 2026-08-05 修复块设备 PCI 地址缺失

- **变更类型**: fix / src / tests
- **涉及文件**: src/reader/linux/sysfs.cpp, src/parser/storage.cpp, tests/unit/test_parse_storage.cpp, docs/devlog.md
- **变更内容**:
  1. `read_block_sysfs`: 采集块设备入口符号链接目标（如 `/sys/block/nvme0n1 -> ../devices/pci0000:e2/0000:e2:04.0/0000:e4:00.0/nvme/nvme0/nvme0n1`）
  2. `parse_storage`: 新增 `extract_pci_address_from_block`，从符号链接目标中取最后一个合法 PCI 地址段（即设备所属的 PCI 控制器）赋给 `dev.pci_address`；虚拟设备（loop/ram）无 PCI 段，静默保持 nullopt
- **原因**: `StorageDevice.pci_address` 字段从未被赋值。与网络设备不同，块设备的 `device` 符号链接（`../../nvme0`）不含 PCI 地址，需从入口符号链接本身提取
- **验证**: `xmake check`（format + tidy + rebuild + test）通过；57/57 存储解析断言通过；本机实测 sda→0000:e3:00.0、nvme1n1→0000:e5:00.0 与真实控制器一致

### 2026-08-03 v0.0.7 收口

- **变更类型**: build / chore
- **涉及文件**: include/sysal/version.hpp, tests/unit/test_serialization.cpp, docs/devlog.md
- **变更内容**: 版本号 0.0.6 → 0.0.7
- **原因**: v0.0.7 发布收口。主要交付：CI 提速（cache 优化）、CHECK 宏 clang-tidy 误报修复、GHCR 镜像构建 + 自动化 Release（glibc 2.17 产物）
- **验证**: `xmake -r` 构建成功

### 2026-07-31 v0.0.6 收口

- **变更类型**: build / chore
- **涉及文件**: include/sysal/version.hpp, tests/unit/test_serialization.cpp, docs/devlog.md
- **变更内容**: 版本号 0.0.5 → 0.0.6
- **原因**: v0.0.6 发布收口。主要交付：脚手架迁移（check.sh → xmake check/test，autoupdate compile_commands，自包含 pre-commit）、clang-format 全量 reformat（ColumnLimit 120, PointerAlignment Right）、clang-tidy 新增 4 项 modernize 检查、删除 set_version、3 项 P1 修复
- **验证**: `xmake -r` 构建成功；18/18 测试通过

### 2026-07-31 修复评审 P1 问题

- **变更类型**: fix / docs
- **涉及文件**: src/parser/execution.cpp, docs/design/data_model/storage.md, docs/design/data_model/raw_store.md, docs/devlog.md
- **变更内容**:
  1. execution.cpp: `container` 环境变量非 docker 值不再一律归为 Podman；新增 `podman` → Podman、`lxc` → Lxc 显式匹配，未知值 → Other（附带原始值）
  2. storage.md: mount_point/fs_type 类型声明从 `std::optional<std::string>` 更新为 `std::optional<MountPoint>`/`std::optional<FilesystemType>`（与代码一致）
  3. raw_store.md: 新增 SysHypervisor 枚举项，条目顺序与代码 enums.hpp 对齐
- **原因**: v0.0.5 代码质量评审 3 项 P1 问题
- **验证**: `xmake -r` 构建成功；18/18 测试通过

### 2026-07-31 v0.0.5 代码质量评审（脚手架迁移后）

- **变更类型**: docs
- **涉及文件**: docs/quality_reports/v005_post_scaffold_review.md, docs/devlog.md
- **变更内容**: 9 维度评审报告，5 Oracle agent，加权总分 8/10（与上次持平）。3 项 P1：container env var 误分类、storage.md 过期、raw_store.md 缺 SysHypervisor。8 项 P2
- **原因**: 脚手架迁移 + clang-format reformat 后全面质量核对
- **验证**: 评审报告完成

### 2026-07-31 更新计划文档 + 删除 set_version

- **变更类型**: docs / build
- **涉及文件**: .sisyphus/plans/v002_r5_plan.md, .sisyphus/plans/v002_dev_plan.md, .sisyphus/plans/version_release.md, xmake.lua, docs/devlog.md
- **变更内容**:
  1. 三个计划文件标记为"全部已完成"，核实 R5a-R5d 在 v0.0.3~v0.0.5 期间已逐步完成
  2. xmake.lua 删除 `set_version()`，版本号只维护 `include/sysal/version.hpp`
- **原因**: 计划文件停留在 v0.0.2 时期，与实际代码状态不符；`set_version()` 与 version.hpp 重复维护
- **验证**: `xmake -r` 构建成功

### 2026-07-30 迁移 base_project 脚手架

- **变更类型**: refactor / build / chore
- **涉及文件**: xmake.lua, .githooks/pre-commit, .github/workflows/ci.yml, .clang-format, .clang-tidy, .clangd, AGENTS.md, docs/devlog.md, utils/check.sh (删除)
- **变更内容**:
  1. xmake.lua: 添加 `plugin.compile_commands.autoupdate` 规则（compile_commands.json 自动更新到 build/），添加 `xmake check` task（format+tidy+rebuild+test）和 `xmake test` task，删除 `set_version()`（版本号只维护 version.hpp），删除头部注释中对 check.sh 的引用，githooks 检测增加 `.git` 文件支持（worktree 兼容）
  2. 删除 `utils/check.sh`（293 行），功能由 `xmake check` task 替代
  3. `.githooks/pre-commit`: 替换为自包含版本（31 行），只修空白+clang-format+git add，不再转发到 check.sh，commit 更快
  4. `.github/workflows/ci.yml`: 改为 `xmake check` + `xmake run sysal_info`
  5. `.clang-format`: ColumnLimit 100→120，PointerAlignment Left→Right，新增 AllowShortFunctionsOnASingleLine/AllowShortIfStatementsOnASingleLine/AllowShortLoopsOnASingleLine/NamespaceIndentation，全量 reformat
  6. `.clang-tidy`: 新增 modernize-use-using、modernize-redundant-void-arg、modernize-loop-convert、readability-const-return-type 检查，保留 sysal 的 4 个额外排除
  7. `.clangd`: 新增 Diagnostics（UnusedIncludes Strict）、InlayHints、Index 配置，保留 sysal 的 Add flags
  8. AGENTS.md: 更新项目结构描述、关键约定（autoupdate、xmake check/test、无 check.sh）
- **原因**: 从 base_project 模板迁移最新脚手架，去掉 293 行 check.sh 脚本，改用 xmake 内置 task，简化维护
- **验证**: `xmake -r` 构建成功；18/18 测试通过；clang-tidy 新增检查零 warning

### 2026-07-30 v0.0.5 收口：版本号更新 + 静态库 fPIC

- **变更类型**: build / chore
- **涉及文件**: include/sysal/version.hpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. 版本号 0.0.4 → 0.0.5（version.hpp + xmake.lua set_version）
  2. 静态库 sysal_static 添加 `-fPIC`，使 libsysal.a 可被链接进下游 .so
- **原因**: v0.0.5 功能开发完成，收口发布。主要交付：CentOS 7 兼容性构建（Docker + glibc 2.17）、硬件虚拟化检测全覆盖、Doxygen 配置、两轮质量评审修复
- **验证**: `xmake -r` 构建成功

### 2026-07-10 引入 Doxygen 文档生成

- **变更类型**: docs / chore
- **涉及文件**: Doxyfile (新增), docs/api_main.md (新增), docs/devlog.md
- **变更内容**:
  1. 新增 Doxyfile，配置 INPUT=include/ + docs/api_main.md，GENERATE_HTML=YES，EXTRACT_PRIVATE=NO，WARN_IF_UNDOCUMENTED=YES
  2. 新增 docs/api_main.md 作为 Doxygen 首页，包含快速开始、核心类型、模块列表、设计原则
- **原因**: API 文档生成准备，Doxygen 注释已就绪，需要配置文件和入口页面
- **验证**: 待 doxygen 安装后生成验证

### 2026-07-10 修复 Doxygen 注释覆盖率与冗余

- **变更类型**: docs
- **涉及文件**: include/sysal/core/collect.hpp, include/sysal/core/error.hpp, include/sysal/core/system.hpp, include/sysal/types/strong_id.hpp, include/sysal/types/value_types.hpp, include/sysal/types/enums.hpp, docs/devlog.md
- **变更内容**:
  1. collect.hpp: operator| 和 has 补 @param + @return；删 10 个枚举值纯翻译注释
  2. error.hpp: SysalError 构造函数补 @brief + @param×2；what() 补 @brief + @return；kind() 补 @brief + @return；删 10 个 ErrorKind 枚举值纯翻译注释
  3. system.hpp: refresh() 补 @throws
  4. strong_id.hpp: 默认构造函数补 @brief；operator==/!=/<< 补 @param + @return
  5. value_types.hpp: PciAddress::operator== 和 NamedString::operator== 补 @param + @return
  6. enums.hpp: 删除 ~25 个纯翻译冗余注释（如 Gpu→GPU、X86_64→x86-64、V1→cgroup v1），保留有信息增益的注释（如 AArch64→ARM 64、None→物理机、Avx512f→AVX-512 Foundation）
- **原因**: 为 API 文档生成做准备，审计发现 31 处 Doxygen 注释缺失和 ~45 处纯翻译冗余注释
- **验证**: `xmake -r` 构建成功（13.4s）

- **变更类型**: refactor / docs
- **涉及文件**: docker/centos7-build/build.sh, docker/centos7-build/build_inside.sh (新增), docker/centos7-build/Dockerfile, README.md, docs/centos7-build.md (新增), docs/devlog.md
- **变更内容**:
  1. 从 build.sh 提取 40 行内联脚本为独立文件 build_inside.sh
  2. build.sh 新增 `--build-arg HTTP_PROXY/HTTPS_PROXY` 传递给 `docker build`（镜像构建时 yum/curl 需要网络）
  3. build.sh 新增 Docker 前置检查（docker 是否安装、daemon 是否运行）
  4. build_inside.sh 删除全部 GLIBC/libstdc++/CXXABI/ldd 符号检查，改为编译后运行 `sysal_info` 验证产物在 CentOS 7 环境下可正常执行
  5. build_inside.sh 用固定路径 `build/linux/x86_64/release/` + 文件存在性断言
  6. Dockerfile xmake 版本提为 `ARG XMAKE_VERSION=3.0.9`
  7. 删除冗余的 `XMAKE_ROOT=y` export（Dockerfile ENV 已设置）
  8. 删除 Dockerfile 多余注释
  9. README 兼容性章节扩展：前提条件、流程说明、文档链接
  10. 新增 docs/centos7-build.md：脚本流程、文件说明、xmake 源码编译原因、vault 源说明、代理支持、已知限制
- **原因**: 原构建脚本内联 40 行 bash 不可维护，无文档，符号检查冗余（能在 CentOS 7 跑起来就是最强证明）
- **验证**: `xmake -r` 构建成功（1.3s，cache hit）

### 2026-07-06 修复 test_replay 环境绑定

- **变更类型**: test / fix
- **涉及文件**: tests/integration/test_replay.cpp, docs/devlog.md
- **变更内容**: test_replay.cpp 移除 lines 91-95 的 live comparison（`System::collect()` 采集本机数据与 fixture 比较 CPU 数量和内存大小），该断言绑定本机环境，在 CI 上必然失败
- **原因**: test_replay 的 live comparison 是定时炸弹——fixture 在开发机上生成，在 CI 机器上 CPU 数量和内存大小不同会导致断言失败
- **验证**: `xmake -r` 构建成功；test_replay 13/13 通过

### 2026-07-05 修复 v0.0.5 评审 P1 问题 + CI 添加 sysal_info

- **变更类型**: fix / test / docs / chore
- **涉及文件**: src/parser/platform.cpp, tests/unit/test_parse_platform.cpp, tests/unit/test_serialization.cpp, docs/design/rules/strong_typing.md, docs/design/testing/serialization.md, .github/workflows/ci.yml, docs/devlog.md
- **变更内容**:
  1. P1 #1: DMI 关键词新增 "Xen" 匹配，覆盖 Xen HVM 检测
  2. P1 #2: KVM 检查移到 QEMU 前面，避免 KVM 客户机被误分类为 QEMU
  3. P1 #3: detect_virtualization 去掉 [[maybe_unused]]，cpuinfo hypervisor flag 命中但 DMI 未匹配时发出 warning
  4. P1 #4: 新增 Parallels 和 Xen HVM 测试场景
  5. P1 #5: 序列化 round-trip 新增 platform.virtualization 断言
  6. P1 #6: strong_typing.md 新增 TransferRate/MountPoint/FilesystemType
  7. P1 #7: serialization.md storage kind 示例从字符串改为整数，meta 版本号更新
  8. CI: 添加 xmake run sysal_info 步骤
- **原因**: v0.0.5 评审报告 7 项 P1 问题修复 + CI 补充 sysal_info 运行验证
- **验证**: `xmake -r` 构建成功；test_parse_platform 72/72、test_serialization 65/65 通过

### 2026-07-05 v0.0.5 代码质量评审

- **变更类型**: docs
- **涉及文件**: docs/quality_reports/v005_review.md, docs/devlog.md
- **变更内容**: v0.0.5 代码质量评审报告（9 维度，5 Oracle agent，加权总分 8/10）。7 项 P1 + 9 项 P2 问题记录
- **原因**: v0.0.5 版本冻结前评审
- **验证**: 评审报告完成，P1 问题包括 Xen HVM 检测 gap、KVM/QEMU 排序、detect_virtualization 零警告、Parallels 未测试、virtualization 未 round-trip、strong_typing.md 缺 3 类型、serialization.md storage kind 示例不一致

### 2026-07-05 v0.0.5 文档冻结：README + 兼容性信息

- **变更类型**: docs
- **涉及文件**: README.md, docs/devlog.md
- **变更内容**:
  1. README.md: Memory 描述更新为"总量、内存类型、配置速率、NUMA、DIMM 拓扑"；新增"兼容性"小节说明 glibc 2.17+ 支持和 CentOS 7 构建方法
  2. devlog: 记录本次文档冻结
- **原因**: v0.0.5 即将冻结版本并进入评审，需确保所有面向用户的文档反映最新功能
- **验证**: 文档审查，README 内容与代码和 AGENTS.md 一致

### 2026-07-05 CentOS 7 兼容性构建 + xmake.lua 工具链自适应

- **变更类型**: build / refactor
- **涉及文件**: xmake.lua, docker/centos7-build/Dockerfile, docker/centos7-build/build.sh, AGENTS.md, docs/devlog.md
- **变更内容**:
  1. xmake.lua: C++23 降级为 C++20（项目实际只使用 C++20 特性），去掉硬编码的 `set_toolchains("clang")` 和 `-stdlib=libc++`/`-fuse-ld=lld`/`-rtlib=compiler-rt`/`-unwindlib=libunwind`，改为在 `on_config` 中检测编译器后条件添加 clang 专属选项
  2. 新增 `docker/centos7-build/Dockerfile`：基于 centos:7，安装 devtoolset-11 (GCC 11.2.1)，从源码编译 xmake v3.0.9，设置 XMAKE_ROOT=y 允许 root 构建
  3. 新增 `docker/centos7-build/build.sh`：在 CentOS 7 容器中编译 sysal，验证 glibc 符号版本 ≤ 2.17
  4. AGENTS.md: 更新工具链描述、项目结构、新增兼容性构建说明
- **原因**: 用户希望预编译产物兼容 glibc 2.17+（CentOS 7 / RHEL 7 及所有主流 Linux 发行版）。原构建环境（Ubuntu + clang + libc++）产出物依赖 GLIBC_2.38（`__isoc23_strtoll`/`__isoc23_strtoull`）和 GLIBCXX_3.4.21，CentOS 7 无法使用。在 CentOS 7 容器中用 GCC 11 编译可天然产出兼容产物
- **验证**: `bash docker/centos7-build/build.sh` 构建成功；产物符号版本：GLIBC 最高 2.14、GLIBCXX 最高 3.4.19、CXXABI 最高 1.3.5，全部在 CentOS 7 (glibc 2.17, GCC 4.8.5 libstdc++) 范围内

### 2026-07-05 文档同步：虚拟化检测 + 内存模型变更后的设计文档对齐

- **变更类型**: docs
- **涉及文件**: docs/design/rules/strong_typing.md, docs/design/testing/serialization.md, docs/design/roadmap.md, docs/issues.md, docs/design/index.md, docs/quality_reports/v004_review.md, docs/devlog.md
- **变更内容**:
  1. `strong_typing.md`: VirtualizationKind 枚举追加 Qemu/HyperV/VirtualBox/Parallels
  2. `serialization.md`: JSON 示例从 DimmInfo 移除 type/configured_speed_mts，Memory 级别新增 memory_type/configured_speed_mts
  3. `roadmap.md`: Memory 描述补充 DIMM 拓扑、内存类型、配置速率
  4. `issues.md`: D-7 描述更新为三源虚拟化检测方案
  5. `index.md`: Memory 索引描述补充 DIMM 详情
  6. `v004_review.md`: 添加脚注说明报告反映 v0.0.4 tag 时状态，后续已变更
- **原因**: 虚拟化检测完善和内存模型精简后，6 个设计文档与代码不一致
- **验证**: 文档审查，确认所有设计文档与当前代码状态一致

### 2026-07-05 精简内存模型：type/configured_speed 提升到 Memory 级别

- **变更类型**: refactor / src / docs / test
- **涉及文件**: include/sysal/model/memory.hpp, src/parser/memory.cpp, src/serialization/serialize.cpp, examples/sysal_info.cpp, tests/unit/test_parse_memory.cpp, tests/unit/test_serialization.cpp, docs/design/data_model/memory.md, docs/devlog.md
- **变更内容**:
  1. `DimmInfo` 移除 `type`（内存代际）和 `configured_speed_mts`（运行频率），这两个字段在同一台服务器上全局一致
  2. `Memory` 新增 `memory_type`（string）和 `configured_speed_mts`（optional<TransferRate>）
  3. `parse_udevadm_dimms` / `parse_edac_dimms` 新增输出参数，将首个 DIMM 的 type 和 configured_speed 提升到 Memory 级别
  4. `serialize.cpp` DimmInfo 序列化移除 type/configured_speed_mts，Memory 序列化新增 memory_type/configured_speed_mts
  5. `sysal_info.cpp` Memory 区显示 Memory type 和 Configured speed，单个 DIMM 不再重复显示
  6. 测试更新：test_parse_memory 和 test_serialization 断言改为 Memory 级别
  7. `memory.md` 更新数据结构定义和设计说明
- **原因**: 同一台服务器只能安装同一代际内存、只能跑同一运行频率。按插槽重复存储 type 和 configured_speed 是冗余的，提升到 Memory 级别更符合物理现实
- **验证**: `xmake -r` 构建成功（0 warnings）；test_parse_memory 75/75、test_serialization 64/64 通过；sysal_info 正确显示 "Memory type: DDR4" 和 "Configured speed (MT/s): 2933" 在 Memory 级别

### 2026-07-05 完善硬件虚拟化检测功能

- **变更类型**: src / fix / refactor
- **涉及文件**: include/sysal/types/enums.hpp, src/reader/linux/sysfs.cpp, src/parser/platform.cpp, src/serialization/serialize.cpp, examples/sysal_info.cpp, tests/unit/test_parse_platform.cpp, docs/design/data_model/platform.md, docs/devlog.md
- **变更内容**:
  1. `VirtualizationKind` 枚举新增 `Qemu`、`HyperV`、`VirtualBox`、`Parallels` 四个值
  2. `RawSource` 枚举末尾新增 `SysHypervisor`（/sys/hypervisor/type）
  3. `sysfs.cpp` 新增 `read_hypervisor_type()` 读取 /sys/hypervisor/type，在 `read_sysfs()` dispatch 中绑定 `Collect::Platform`
  4. `platform.cpp` 重写 `detect_virtualization()`：删除旧的 /proc/1/cgroup "kvm" 字符串匹配，改为多源检测——优先级 1: /sys/hypervisor/type=="xen" → Xen；优先级 2: DMI sys_vendor/product_name 关键词匹配（VMware/Hyper-V/QEMU/Bochs/VirtualBox/Parallels/KVM，大小写不敏感）；优先级 3: /proc/cpuinfo flags 含 `hypervisor` 标志 → Other/Unknown
  5. `serialize.cpp` `validate_enum` 的 RawSource 上界从 `SysfsEdac` 改为 `SysHypervisor`
  6. `sysal_info.cpp` `virt_kind_str()` 补全新枚举值的 case，`raw_source_str()` 补 `SysHypervisor`，虚拟化未检测到时显示 "Physical (no virtualization detected)"
  7. `test_parse_platform.cpp` 测试 3 从 /proc/1/cgroup "kvm" 改为 DMI sys_vendor="KVM" 检测；新增测试 4-11 覆盖 Xen/VMware/Hyper-V/QEMU/VirtualBox/cpuinfo hypervisor flag/物理机/优先级场景
  8. `platform.md` 更新 Virtualization 枚举值列表和检测优先级说明
- **原因**: 旧实现仅检查 /proc/1/cgroup 里的 "kvm" 字符串，极不可靠（cgroup 路径名与硬件虚拟化无关）。新实现基于 /sys/hypervisor/type、DMI 厂商/产品名、CPU hypervisor flag 三源检测，覆盖主流 hypervisor
- **验证**: `xmake -r` 构建成功（0 warnings，-Wall -Wextra -Werror）；`xmake run test_parse_platform` 63/63 通过；全部 18 个测试通过（758 assertions）；`xmake run sysal_info` 物理机正确显示 "Physical (no virtualization detected)"

### 2026-07-05 修复全部 P1+P2 评审问题 + README 重写 + 评审 Skill + CI 修复

- **变更类型**: fix / refactor / docs / test / chore
- **涉及文件**: include/sysal/types/units.hpp, value_types.hpp, include/sysal/model/memory.hpp, storage.hpp, src/parser/memory.cpp, network.cpp, storage.cpp, platform.cpp, pci.cpp, cpu.cpp, parse_utils.hpp, parse_utils.cpp, src/pipeline/pipeline.cpp, src/serialization/serialize.cpp, examples/sysal_info.cpp, tests/unit/test_parse_memory.cpp, test_parse_storage.cpp, test_parse_network.cpp, test_parse_platform.cpp, test_serialization.cpp, test_reader.cpp, tests/integration/test_replay.cpp, tests/fixtures/dev_machine.json, docs/design/public_api.md, README.md, .github/workflows/ci.yml, .opencode/skills/code-quality-review/SKILL.md, docs/devlog.md
- **变更内容**:
  **P1 修复（7 项）**:
  1. DimmInfo.speed_mts/configured_speed_mts 改用 TransferRate ScalarUnit（新建 tag）
  2. DimmInfo.manufacturer 改用 optional<Vendor>
  3. StorageDevice.mount_point/fs_type 改用 MountPoint/FilesystemType NamedString（新建 tag）
  4. record_collector_status 加 flags 参数，只检查请求域
  5. parse_numa_meminfo 去掉 [[maybe_unused]]，NUMA 解析失败加 warning
  6. test_serialization round-trip 加 DimmInfo/mount_point/fs_type/addresses/pci_address 断言
  7. test_reader 加 ProcHostname/IfAddrs/DfTh 检查
  **P2 修复（13 项）**:
  1. 提取 extract_filename 到 parse_utils，删 3 份重复
  2. cpu.cpp (void) cast 改为 [[maybe_unused]]
  3. pipeline.cpp backend init/shutdown 加 RAII guard
  4. udevadm/IfAddrs/df 畸形行加 warning
  5. platform.cpp hostname 缺失加 warning
  6. storage 分区匹配改为优先选 "/" 根挂载
  7. pci.cpp 删死代码（parse_uint 无法解析 "-1" 的分支）
  8. storage 分区前缀匹配加数字边界检查
  9. serialize.cpp RawSource/CollectStatus 加 validate_enum
  10. public_api.md 修 full|Collect::Raw 示例矛盾
  11. memory.hpp 文件头注释加 DimmInfo
  12. test_replay 替换 CHECK(true) 占位
  13. 3 个 parser 加畸形输入测试
  **其他**:
  - README 重写（简洁叙事体，面向开发者）
  - 创建 .opencode/skills/code-quality-review/SKILL.md
  - CI 移除多余的 sysal_info 单独 step（check.sh 已覆盖 build+tests）
  - clang-format 修复（execution.cpp, cpu.cpp, pci.cpp）
  - 推送 v0.0.2 和 v0.0.3 tag 到远程
- **原因**: v0.0.4 代码质量评审发现的全部 P1+P2 问题修复；README 面向开发者精简化；评审方法封装为 skill 便于复用；CI 清理
- **验证**: `xmake -r` 构建成功（0 warnings）；18/18 测试通过（729 assertions）

### 2026-07-05 修复测试完整性问题（序列化往返、reader 覆盖、存储分区匹配、replay 占位、畸形输入）

- **变更类型**: tests / src / fix
- **涉及文件**: tests/unit/test_serialization.cpp, tests/unit/test_reader.cpp, tests/unit/test_parse_network.cpp, tests/unit/test_parse_storage.cpp, tests/unit/test_parse_memory.cpp, tests/integration/test_replay.cpp, src/parser/storage.cpp, tests/fixtures/dev_machine.json
- **变更内容**:
  1. `test_serialization.cpp`: `test_round_trip()` 新增 v0.0.4 字段往返校验——Memory 的 dimms/dimm_count/populated_dimms 及首条 DIMM 的 type/size/speed_mts/manufacturer；Storage 首个含 mount_point 设备的 mount_point/fs_type 往返；Network 首个含 addresses 接口的地址列表往返、首个含 pci_address 接口的 PCI 地址往返；均用 has_value() 守卫
  2. `test_reader.cpp`: procfs 采集后新增 `CHECK(raw.has(ProcHostname))`、`CHECK(raw.has(IfAddrs))`、`CHECK(raw.has(DfTh))` 三项断言
  3. `storage.cpp`: 分区匹配逻辑移除 `break`，遍历所有匹配分区并优先选择挂载点为 "/" 的根分区（原逻辑仅取第一个匹配）
  4. `test_parse_storage.cpp`: 测试 8 期望值从 `/boot/efi`+`vfat` 改为 `/`+`ext4`（根分区优先）；新增测试 10 验证 df -Th 畸形输入（字段不足行）不崩溃且产生警告
  5. `test_parse_network.cpp`: 新增测试 9 验证 IfAddrs 畸形输入（无空格行、空行）不崩溃且产生警告
  6. `test_parse_memory.cpp`: 新增测试 9 验证 udevadm 畸形输入（不可解析索引、缺字段名）不崩溃且产生警告
  7. `test_replay.cpp`: 第 61 行 `CHECK(true)` 替换为 `CHECK(!sys.info.platform.host.hostname.empty())`；第 92 行 `CHECK(true)` 及注释删除
  8. `tests/fixtures/dev_machine.json`: 重新生成（含 ProcHostname 等新 RawSource 记录）
- **原因**: 审计发现 v0.0.4 新增字段缺少往返校验、5 个新 RawSource 类型无 reader 覆盖、存储分区匹配仅取首匹配导致根分区丢失、replay 存在 CHECK(true) 占位、新解析器无畸形输入测试
- **验证**: `xmake -r` build ok 0 warnings；全部 18 个测试套件通过（test_serialization 61、test_reader 29、test_parse_network 50、test_parse_storage 57、test_parse_memory 77、test_replay 15，均 0 failed）；clang-format --Werror 全部通过

### 2026-07-05 procfs/platform 改用系统调用替代文件与命令采集

- **变更类型**: src / tests
- **涉及文件**: src/reader/linux/procfs.cpp, src/parser/platform.cpp, tests/unit/test_parse_platform.cpp
- **变更内容**:
  1. `procfs.cpp`: 新增 `read_uname()` 辅助函数，一次 `uname()` 调用同时填充 `RawSource::Uname`（payload=`buf.machine`）与 `RawSource::ProcVersion`（payload=`"release\nversion"`）两条记录；新增 `read_hostname()` 调用 `gethostname()` 填充 `RawSource::ProcHostname`；Platform 域中原 `read_proc_file(/proc/version)`、`read_cmd("uname -m")`、`read_proc_file(/proc/sys/kernel/hostname)` 三行替换为 `read_uname()` + `read_hostname()` 两次调用；新增 `<sys/utsname.h>`、`<unistd.h>` 头文件
  2. `platform.cpp`: `parse_proc_version()` 重写为解析新的两行格式（行1=release→kernel.release，截取首个 `-` 前为 kernel.version；行2=version→kernel.compiled_at 完整字符串）；移除原 `Linux version` 前缀解析与 `#` 后星期几时间戳提取逻辑；移除不再使用的 `<algorithm>`、`<array>` 头文件
  3. `test_parse_platform.cpp`: 全部 ProcVersion 测试载荷从旧 free-form `/proc/version` 字符串改为 `"release\nversion"` 两行格式，path_or_command 从 `/proc/version` 改为 `uname`；Uname 记录 path_or_command 从 `uname -m` 改为 `uname`；测试 1 的 compiled_at 期望值更新为完整 version 字符串（含 `#101-Ubuntu SMP ` 前缀）
- **原因**: 审计发现 procfs 采集层存在 3 处可通过系统调用替代的文件/命令读取（`popen("uname -m")`、`read_file("/proc/sys/kernel/hostname")`、`read_file("/proc/version")`）；系统调用比 fork+exec 或文件读取更轻量、更可靠，且一次 `uname()` 可同时提供架构与内核信息
- **验证**: `xmake -r` build ok 0 warnings；`xmake run test_parse_platform` 34 passed 0 failed；`xmake run test_reader` 26 passed 0 failed；`xmake run test_replay` 16 passed 0 failed；`xmake run sysal_info` 正确显示 hostname=head、kernel release=6.8.0-111-generic、version=6.8.0、arch=x86_64

### 2026-07-05 内存 DIMM 详细信息采集

- **变更类型**: src / tests
- **涉及文件**: include/sysal/model/memory.hpp, src/reader/linux/sysfs.cpp, src/parser/memory.cpp, src/serialization/serialize.cpp, examples/sysal_info.cpp, tests/unit/test_parse_memory.cpp
- **变更内容**:
  1. `memory.hpp`: 新增 `DimmInfo` 结构体（locator/bank_locator/size/type/speed_mts/configured_speed_mts/manufacturer/part_number/rank/total_width/data_width/form_factor/present）；`Memory` 新增 `dimms`、`dimm_count`、`populated_dimms` 字段
  2. `sysfs.cpp`: 新增 `read_edac_sysfs()`，遍历 `/sys/devices/system/edac/mc/mcN/dimmM/` 读取 dimm_mem_type/size/dimm_label/dimm_location/dimm_dev_type/dimm_edac_mode，存为 `RawSource::SysfsEdac`；无 EDAC 目录时静默跳过；在 `Collect::Memory` 下分派
  3. `memory.cpp`: 新增 `parse_udevadm_dimms()` 解析 udevadm `MEMORY_DEVICE_N_FIELD=value` 条目，按 N 分组构建 DimmInfo；PRESENT 字段缺失时按 SIZE>0 推断 present；新增 `parse_edac_dimms()` 作为 udevadm 无结果时的回退，按 sysfs 路径分组，size 从 MB 转字节；`parse_memory()` 中 udevadm 优先、EDAC 回退，并计算 dimm_count/populated_dimms
  4. `serialize.cpp`: 新增 `dimm_info_to_json`/`dimm_info_from_json`，`memory_to_json`/`memory_from_json` 增加 dimms/dimm_count/populated_dimms 字段
  5. `sysal_info.cpp`: section "4. Memory" 增加 DIMM 插槽总数、已安装数及逐条 DIMM 详情输出
  6. `test_parse_memory.cpp`: 新增 3 个测试用例（udevadm 2 DIMM 一空一满、EDAC 回退 2 DIMM、无数据时 dimms 为空不崩溃）
- **原因**: Memory 模型此前仅有总量与 NUMA 分布，缺少 DIMM 级别详情；reader 层 procfs 已采集 udevadm，sysfs 需补充 EDAC 读取作为回退
- **验证**: `xmake -r` build ok；`xmake run test_parse_memory` 67 passed 0 failed；`xmake run test_serialization` 37 passed 0 failed；全部 18 个测试套件通过；sysal_info 正确显示 27 DIMM 插槽 / 11 已安装

### 2026-07-05 网络接口 IP 地址与 PCI 地址解析

- **变更类型**: src / tests
- **涉及文件**: src/reader/linux/sysfs.cpp, src/parser/network.cpp, tests/unit/test_parse_network.cpp
- **变更内容**:
  1. `sysfs.cpp` `read_net_sysfs()`: 读取 `/sys/class/net/<ifname>/device` 符号链接目标，存为 SysfsNet 记录（payload 为符号链接目标字符串）；虚拟接口无此链接时静默跳过
  2. `network.cpp` `parse_network()`: 解析 IfAddrs 记录（payload 格式 "ifname ip\n"），按接口名分组填充 `NetworkInterface.addresses`
  3. `network.cpp` `parse_network()`: 处理 SysfsNet 中 filename=="device" 的记录，提取符号链接目标末尾路径分量，经 `parse_pci_address()` 解析后填充 `NetworkInterface.pci_address`
  4. `test_parse_network.cpp`: 新增 4 个测试用例（IfAddrs IPv4/IPv6 填充、device 符号链接 PCI 地址填充、无 IfAddrs 时空 addresses、虚拟接口无 PCI 地址）
- **原因**: NetworkInterface 模型已有 addresses 与 pci_address 字段但 parser 未填充；reader 层 procfs 已采集 getifaddrs 数据，sysfs 需补充 device 符号链接读取
- **验证**: `xmake -r` build ok；`xmake run test_parse_network` 44 passed 0 failed；`xmake run test_reader` 26 passed 0 failed；test_replay/test_collect/test_serialization 均通过

### 2026-07-02 消除 sysal_info 全部 warnings

- **变更类型**: src / fix / tests
- **涉及文件**: src/parser/storage.cpp, src/parser/software.cpp, tests/unit/test_parse_software.cpp, tests/unit/test_parse_storage.cpp
- **变更内容**:
  1. `storage.cpp`: 移除 B-2 PCI 地址 TODO warning（功能未实现，不应产生运行时噪声）
  2. `software.cpp` `extract_nvidia_driver_version()`: 从搜索 `"Driver Version:"` 表格文本改为从 CSV 第 5 列提取 driver_version（匹配 reader 的 `--query-gpu=...,driver_version --format=csv,noheader` 命令）
  3. `test_parse_software.cpp`: 测试 payload 从表格格式更新为 CSV 格式
  4. `test_parse_storage.cpp`: 移除 B-2 warning 断言，改为设备属性检查
- **原因**: sysal_info 报告 6 条 warnings（5 条 B-2 TODO + 1 条 nvidia-smi 解析失败），均为 parser 与 reader 不匹配或未实现功能产生的噪声
- **验证**: `xmake -r` build ok；全部 18 个测试通过；sysal_info 报告 0 warnings

### 2026-07-02 v0.0.3 版本号升级 + 设计文档对齐

- **变更类型**: docs / chore
- **涉及文件**: include/sysal/version.hpp, README.md, tests/unit/test_serialization.cpp, docs/design/rules/strong_typing.md, docs/design/data_model/platform.md, docs/design/testing/serialization.md, docs/design/architecture/pipeline.md, docs/design/data_model/raw_store.md
- **变更内容**:
  1. `version.hpp`: 版本号从 0.0.2 升级至 0.0.3
  2. `test_serialization.cpp`: 内联 JSON sysal_version 同步更新为 "0.0.3"
  3. `README.md`: 版本号更新为 v0.0.3
  4. `strong_typing.md`: StorageKind 枚举更新为 {Nvme, Ssd, Hdd, Other}；IsaExtension 枚举更新为 17 个值
  5. `platform.md`: Virtualization 表行移除 `container（bool）` 字段
  6. `serialization.md`: "手写 JSON 引擎" 描述更新为 nlohmann/json
  7. `pipeline.md`: 源文件布局移除已删除的 `json.hpp`
  8. `raw_store.md`: RawSource 枚举补充 `ProcHostname`
- **原因**: v0.0.2/v0.0.3 代码变更后设计文档存在 5 处过时引用
- **验证**: grep 确认 docs/design/ 中无 Sata/Sas/json.hpp/手写/container(bool) 残留

### 2026-07-02 删除冲突解决死代码 + lspci warning + 序列化 round-trip 扩展

- **变更类型**: src / tests
- **涉及文件**: src/resolver/resolve.cpp, src/parser/pci.cpp, tests/unit/test_serialization.cpp
- **变更内容**:
  1. `resolve.cpp`: 删除 `TrustLevel` 枚举、`resolve_conflict` 函数及两个 `#pragma clang diagnostic` 守卫（dead code，从未被调用）；删除 `resolve()` 中的"冲突解决框架"注释块
  2. `pci.cpp`: lspci 独有设备（无 sysfs 对应）添加 warning `"lspci 独有设备，sysfs 数据缺失"`
  3. `test_serialization.cpp` `test_round_trip()`: 扩展 collect flags 覆盖 Accelerator/Network/Storage/Pci；新增 round-trip 断言从 6 个增至 37 个，覆盖 accelerator（name/kind/memory_size）、storage（name/kind/capacity）、pci（address/vendor）、network（name/state）、platform（kernel.release/architecture）、meta（collect_duration/requested_flags）
- **原因**: v0.0.2 评审 P0（冲突解决死代码）+ P2（lspci 部分填充无提示 + 序列化 round-trip 覆盖率低）
- **验证**: `xmake -r` build ok；test_resolve 34 passed；test_parse_pci 40 passed；test_serialization 37 passed

### 2026-07-02 修复 kernel version 语义 + storage 虚拟设备 + capabilities 解码

- **变更类型**: src / fix / tests
- **涉及文件**: src/parser/platform.cpp, src/parser/storage.cpp, src/parser/execution.cpp, tests/unit/test_parse_platform.cpp, tests/unit/test_parse_execution.cpp
- **变更内容**:
  1. `platform.cpp` `parse_proc_version()`: `version` 改为从 `release` 截取 `-` 前的上游版本号（如 "6.8.0"），`compiled_at` 为时间戳（如 "Sat Apr 11 ..."）
  2. `storage.cpp` `infer_storage_kind()`: 新增虚拟设备前缀过滤（loop/ram/sr/dm-/md/zram/fd），虚拟设备归为 `Other` 不按 rotational 分类
  3. `execution.cpp`: 新增 `decode_capabilities()` 函数，将 CapEff 十六进制位掩码解码为人类可读名称（CAP_CHOWN/CAP_NET_ADMIN 等 40 个 Linux capability）；`is_root` 修复：仅当 euid==0 且 Uid 行实际被解析时才为 true
  4. `test_parse_platform.cpp`: kernel version 断言更新为 "5.15.0"
  5. `test_parse_execution.cpp`: capabilities 断言从 `"000001ff"` 更新为 8 个 CAP_ 名称
- **原因**: kernel version 语义应为上游版本号而非 #构建字符串；loop 设备 rotational=1 被误报为 HDD；capabilities 输出原始 hex 无意义；is_root 在无 Uid 行时默认 0 误报
- **验证**: `xmake -r` build ok；全部测试通过；sysal_info 输出 "Kernel version: 6.8.0"、"loop0 (Other)"、"sda (HDD)"、UEFI: yes

### 2026-07-01 testbench 重命名为 sysal_info + 统一 CHECK 宏

- **变更类型**: refactor / tests / build / docs
- **涉及文件**: examples/sysal_info.cpp (git mv from tests/testbench.cpp), tests/test_macros.hpp (新增), tests/unit/*.cpp (16 个), tests/unit/test_serialization.cpp, tests/integration/test_replay.cpp, xmake.lua, README.md, AGENTS.md, docs/design/testing/raw_replay.md, docs/devlog.md
- **变更内容**:
  1. `tests/testbench.cpp` 经 `git mv` 移至 `examples/sysal_info.cpp`，文件头注释从 "testbench / 全量能力测试" 改为 "sysal_info / 全量能力演示"，输出 banner 从 "=== sysal testbench ===" 改为 "=== sysal info ==="
  2. xmake.lua: `test_target("testbench", ...)` 替换为独立 `target("sysal_info")`（直接定义，不走 test_target 辅助函数，因 examples/ 非 tests/）；`task("testbench")` 重命名为 `task("sysal_info")`；`test_target` 辅助函数新增 `add_includedirs("tests")` 以便测试文件找到 test_macros.hpp
  3. 新增 `tests/test_macros.hpp`：定义 `CHECK(expr)` 宏（失败时 fprintf 到 stderr 并 ++g_test_fail，成功 ++g_test_pass）和 `TEST_SUMMARY()` 宏（打印通过/失败计数并返回退出码）
  4. test_serialization.cpp: 移除本地 g_pass/g_fail/check()/CHECK 定义，改为 `#include "test_macros.hpp"`；`++g_pass` 改为 `CHECK(true)`；末尾 `return g_fail == 0 ? 0 : 1` 改为 `TEST_SUMMARY()`
  5. test_replay.cpp: 移除本地 2 参数 `CHECK(cond, msg)` 宏定义，改为 `#include "test_macros.hpp"`；全部 `CHECK(cond, msg)` 转为单参数 `CHECK(cond)`；末尾 `return 0` 改为 `TEST_SUMMARY()`
  6. 16 个 assert 测试文件（test_types/test_model/test_parse_utils/test_parse_platform/test_parse_cpu/test_parse_memory/test_parse_accelerator/test_parse_storage/test_parse_pci/test_parse_network/test_parse_software/test_parse_execution/test_reader/test_resolve/test_collect/test_raw_store_io）：`#include <cassert>` 替换为 `#include "test_macros.hpp"`，全部 `assert(expr)` 替换为 `CHECK(expr)`（AST 感知替换，不影响 static_assert），末尾 `return 0;` 替换为 `TEST_SUMMARY()`
  7. README.md: "运行 testbench" 章节改为 "运行 sysal_info"，`xmake testbench` / `xmake run testbench` 改为 `xmake sysal_info` / `xmake run sysal_info`
  8. AGENTS.md: `xmake testbench` 改为 `xmake sysal_info`
  9. docs/design/testing/raw_replay.md: fixture 布局中 `testbench.cpp` 引用改为 `examples/sysal_info.cpp`
- **原因**: testbench 实为 demo 而非测试，移至 examples/ 并重命名为 sysal_info 以正名；16 个测试文件使用 assert() 在 NDEBUG 下会被编译消除（虽已 -UNDEBUG 防护），统一为 CHECK 宏提供持续计数和汇总输出，测试结果更清晰
- **验证**: `xmake -r` 构建零 warning 零 error；全部 18 个测试通过（test_parse_cpu 72 passed / test_types 16 passed / test_serialization 14 passed / test_replay 16 passed 等）；`xmake sysal_info` 运行正常，输出 17 section 完整内容

### 2026-07-01 修复 3 个代码审查问题

- **变更类型**: src / fix / tests
- **涉及文件**: include/sysal/model/platform.hpp, src/parser/platform.cpp, src/parser/cpu.cpp, src/parser/storage.cpp, src/parser/network.cpp, src/parser/accelerator.cpp, src/parser/memory.cpp, src/parser/execution.cpp, src/parser/software.cpp, src/parser/pci.cpp, src/serialization/serialize.cpp, tests/unit/test_parse_platform.cpp, tests/testbench.cpp
- **变更内容**:
  1. **FIX 1 (P1)**: `Firmware::bios_vendor` 类型从 `std::string` 改为 `Vendor`（NamedString），与 `Host::vendor` 保持一致；`platform.cpp` 解析处改用 `Vendor{trim(payload)}`；`serialize.cpp` 序列化/反序列化改用 `.value`；`test_parse_platform.cpp` 和 `testbench.cpp` 断言改用 `.value`
  2. **FIX 2 (P2)**: 所有解析器在遍历原始记录时跳过 `CollectStatus != Success` 的记录。单记录场景（ProcCpuInfo、EtcOsRelease、ProcVersion、Uname、ProcHostname、ProcMemInfo、ProcSelfStatus、ProcSelfCgroup、Environment、NvidiaSmi、Nvcc）改为查找首条 Success 记录；多记录遍历场景（SysfsCpu、SysfsNuma、SysfsDmi、ProcOneCgroup、SysfsBlock、SysfsNet、SysfsPci）在循环顶部 `continue` 跳过非 Success 记录；`pci.cpp` 的 SysfsPci 分组循环新增状态检查（lspci 已有）
  3. **FIX 3 (P2)**: `serialize.cpp` 新增 `validate_enum` 模板函数，反序列化时校验枚举值范围；替换 8 处 `static_cast<Enum>` 为 `validate_enum(...)`，覆盖 Arch、InterfaceState、StorageKind、AcceleratorKind、IsaExtension、VirtualizationKind、CgroupVersion、ContainerKind
- **原因**: v0.0.1 审查遗留问题修复——bios_vendor 类型不一致、解析器未过滤失败记录、JSON 反序列化缺少枚举范围校验
- **验证**: `xmake -r` 构建零 warning 零 error；全部 18 个测试通过（test_parse_platform、test_serialization、test_parse_storage 等）

### 2026-07-01 R6 testbench 输出修复 + tests/ 目录重组

- **变更类型**: src / fix / tests / chore
- **涉及文件**: src/parser/platform.cpp, src/reader/linux/sysfs.cpp, tests/testbench.cpp, tests/unit/test_parse_platform.cpp, xmake.lua, docs/devlog.md, tests/{unit,integration}/* (git mv)
- **变更内容**:
  1. `platform.cpp` `parse_proc_version()`：将基于 `" SMP"` 的版本/时间戳切分改为基于星期几名称（Mon/Tue/Wed/Thu/Fri/Sat/Sun）定位时间戳起点，正确处理 `#111-Ubuntu SMP PREEMPT_DYNAMIC Sat Apr 11 ...` 这类含 PREEMPT_DYNAMIC 的现代内核版本字符串；新增 `#include <array>`
  2. `testbench.cpp` `format_frequency()`：GHz/MHz 改为 `%.2f` 浮点格式（如 "2.20 GHz"），原整数除法丢失小数
  3. `testbench.cpp` `format_memory()`：GiB/MiB 改为 `%.2f` 浮点格式（如 "80.00 GiB"），原整数除法丢失小数；新增 `#include <cstdio>`
  4. UEFI 检测：`sysfs.cpp` `read_dmi_sysfs()` 末尾检查 `/sys/firmware/efi` 存在性并写入 SysfsDmi 记录；`platform.cpp` `parse_dmi()` 记录循环中匹配 `/sys/firmware/efi` 路径置 `firmware.uefi = true`，移除原硬编码 `false`
  5. `test_parse_platform.cpp` 测试 1 内核版本断言从 `#101-Ubuntu` 更新为 `#101-Ubuntu SMP`（匹配新解析逻辑）
  6. tests/ 目录重组：17 个单元测试 .cpp 经 `git mv` 移入 `tests/unit/`，`test_replay.cpp` 移入 `tests/integration/`，`testbench.cpp` 与 `fixtures/` 保留原位；`xmake.lua` 测试源路径同步更新
  7. `test_serialization.cpp` test_compatible_version 内联 JSON 的 sysal_version 从 "0.0.1" 更新为 "0.0.2"（版本号 bump 后遗漏）
- **原因**: testbench 输出存在 4 个 bug（内核版本截断、频率/显存整数除法丢精度、UEFI 永远 false）；tests/ 扁平结构随测试增多需按 unit/integration 分层
- **验证**: `xmake -r` build ok；`test_parse_platform` 通过；`testbench` 退出码 0，输出 "Kernel version: #111-Ubuntu SMP PREEMPT_DYNAMIC"、"Base freq: 2.20 GHz"、"Memory: 80.00 GiB"、"UEFI: yes"；全部测试通过（含 test_serialization sysal_version 断言已同步更新为 0.0.2）

### 2026-07-01 R5b ISA 扩展枚举扩展 + 版本号升级至 0.0.2

- **变更类型**: src / tests / chore
- **涉及文件**: include/sysal/types/enums.hpp, src/parser/cpu.cpp, tests/test_parse_cpu.cpp, include/sysal/version.hpp, docs/devlog.md
- **变更内容**:
  1. `IsaExtension` 枚举从 8 个扩展到 17 个：新增 `Sse, Sse2, Sse3, Ssse3, Sse41, Aes, Fma, F16c, Pclmulqdq`；枚举顺序按 ISA 代际排列（SSE→AVX→AVX-512→其他）
  2. `cpu.cpp` `isa_map` 查找表同步扩展，匹配 /proc/cpuinfo flags 字段名（`sse`, `sse2`, `sse3`, `ssse3`, `sse4_1`, `sse4_2`, `aes`, `fma`, `f16c`, `pclmulqdq`）
  3. `test_parse_cpu.cpp` 新增测试 8：验证全部 17 个 ISA 扩展的解析与顺序
  4. `version.hpp` 版本号从 `0.0.1` 升级至 `0.0.2`
- **原因**: 原 IsaExtension 仅覆盖 AVX 系列，缺少 SSE 系列和 AES/FMA 等常见扩展；版本号随 v0.0.2 功能集更新
- **验证**: `xmake -r` build ok；全部测试通过

### 2026-07-01 R5c 解析 lspci 输出填充 PciDevice.device_name 人类可读名

- **变更类型**: src / tests
- **涉及文件**: src/parser/pci.cpp, tests/test_parse_pci.cpp, docs/devlog.md
- **变更内容**:
  1. `pci.cpp` 新增 `normalize_lspci_address()`：将 lspci 短格式 `BB:DD.F` 补域名前缀 `0000:` 归一化为 `DDDD:BB:DD.F`，长格式保持不变
  2. `pci.cpp` 新增 `parse_lspci_line()`：用 string find/substr（非 regex）解析单行 `lspci -nn` 输出，提取归一化地址与设备名；设备名定位靠 `]: ` 分隔类名段、再以 `rfind('[')` 找最后一个含 `vendor:device` 十六进制对的方括号，取其前内容，正确处理设备名自身含方括号的情况（如 `[GeForce GTX 1080 Ti]`）
  3. `parse_pci()` 在 sysfs 解析后增加 lspci 合并阶段：按地址匹配，已有 sysfs 设备则用 lspci 名覆盖 `device_name`；lspci 独有设备（无 sysfs）则新增到 `pci.devices`（仅地址与名称已知）
  4. `test_parse_pci.cpp` 新增测试 5/6/7/8：覆盖 lspci 名覆盖 sysfs hex、名称含方括号带 rev、lspci 独有设备入列、带域名 `DDDD:BB:DD.F` 格式
- **原因**: sysfs `device` 文件仅给 hex ID（如 `0x1e04`），非人类可读；`lspci -nn` 已由 reader 采集到 `RawSource::Lspci`，需在 parser 层合并以填充 `PciDevice.device_name` 为人类可读名
- **验证**: `xmake -r` build ok；`test_parse_pci` 通过；`testbench` 退出码 0，PCI 段输出真实人类可读名（如 "Intel Corporation Ice Lake Memory Map/VT-d"、"Intel Corporation Ice Lake CBDMA [QuickData Technology]"）

### 2026-07-01 R5a 存储 HDD/SSD 识别 + R5d 容器设计清理

- **变更类型**: src / refactor / tests
- **涉及文件**: src/reader/linux/sysfs.cpp, src/parser/storage.cpp, src/parser/platform.cpp, src/serialization/serialize.cpp, include/sysal/model/platform.hpp, tests/test_parse_storage.cpp, tests/test_parse_platform.cpp, tests/testbench.cpp, docs/devlog.md
- **变更内容**:
  1. R5a：`read_block_sysfs()` 新增读取 `/sys/block/<dev>/queue/rotational`（"0"=SSD, "1"=HDD）
  2. R5a：`infer_storage_kind()` 签名改为接受 `rotational` 参数；新逻辑：nvme 前缀→Nvme，rotational="0"→Ssd，"1"→Hdd，无数据→Other；移除旧的 `sd→Sata` 映射
  3. R5a：`parse_storage()` 从 `device_attrs` 取出 `rotational` 并 trim 后传入 `infer_storage_kind`
  4. R5a：`test_parse_storage.cpp` 更新测试 1（sda rotational=1→Hdd），新增测试 5/6/7 覆盖 rotational=0→Ssd、nvme 无 rotational→Nvme、sd 无 rotational→Other
  5. R5d：`Virtualization` 结构体移除 `bool container` 字段
  6. R5d：`detect_virtualization()` 移除 docker/kubepods cgroup 检测与 `/.dockerenv` 检测，仅保留 KVM 硬件虚拟化检测；容器信息统一由 `ExecutionContext.container` 承载
  7. R5d：`virt_to_json`/`virt_from_json` 移除 `container` 字段（旧 JSON 含该字段将被 nlohmann::json 默认忽略）
  8. R5d：`test_parse_platform.cpp` 测试 2 改为断言容器环境不再产生 Virtualization
  9. 修复 `testbench.cpp` 中 `storage_kind_str`（Sata/Sas→Ssd/Hdd）与 `isa_str`（补全 9 个缺失的 IsaExtension case，消除 -Wswitch -Werror）
- **原因**: StorageKind 枚举已重构（移除 Sata/Sas，新增 Ssd/Hdd），需基于 sysfs rotational 标志正确分类；Virtualization.container 与 ExecutionContext.container 职责重叠，移除前者以统一容器信息归属
- **验证**: `xmake -r` build ok；全部 19 个测试通过（test_parse_storage / test_parse_platform / test_serialization 及其余）；testbench 退出码 0

### 2026-07-01 check.sh 排除 vendored 第三方代码

- **变更类型**: chore
- **涉及文件**: utils/check.sh
- **变更内容**: pre-commit hook 模式下，从 STAGED_FILES 和 STAGED_CPP_FILES 中过滤掉 `third_party/` 路径，避免对 vendored nlohmann/json 头文件执行 clang-format/clang-tidy
- **原因**: vendored 第三方库代码不应被项目 lint 规则检查，否则 clang-tidy 会报大量 warning/error 导致 pre-commit 失败
- **验证**: `git commit` pre-commit hook 通过

### 2026-07-01 用 nlohmann/json 替换手写 JSON 引擎

- **变更类型**: src / refactor / build / chore
- **涉及文件**: src/serialization/serialize.cpp, src/serialization/json.hpp（删除）, xmake.lua, third_party/nlohmann/（新增 vendored 头文件）, docs/devlog.md
- **变更内容**:
  1. 删除 745 行手写 JSON 引擎 `src/serialization/json.hpp`（JsonObj/JsonArr/JsonVal/JsonParser/escape_string/dump_json 等全部移除）
  2. 重写 1942 行 `src/serialization/serialize.cpp`（约 1490 行），使用 nlohmann/json 库替代手写引擎。所有模型类型（Platform/Cpu/Memory/Accelerator/Network/Storage/Pci/Software/Execution/SystemInfo/SnapshotMeta/RawStore）的 to_json/from_json 转换函数改为基于 `nlohmann::json` 实现
  3. 公共 API（`to_json`/`from_json`/`save_raw_store`/`load_raw_store`）签名与行为保持不变；JSON 格式完全兼容（字段名、枚举整数值、StrongId 整数值、NamedString 字符串值、ScalarUnit uint64 值、optional 字段省略规则、版本兼容性检查均不变）
  4. 错误处理：捕获 `nlohmann::json::exception` 并重新抛出为 `SysalError(DeserializationError, ...)`
  5. xmake.lua：添加 `add_includedirs("third_party")` 引入 vendored nlohmann/json 头文件（因 GitHub 不可达，无法通过 `add_requires("nlohmann_json")` 从 xrepo 下载，改为 vendor 到 `third_party/nlohmann/`）
  6. xmake.lua：从 `unit_tests` 列表移除 `test_json` 并删除 `tests/test_json.cpp`（该测试直接测试已删除的手写 JSON 引擎内部 API）
- **原因**: 用成熟的外部库替代手写 JSON 引擎，减少维护负担，提升健壮性（UTF-8 处理、数字精度、边缘 case 等）
- **验证**: `xmake -r` build ok；`test_serialization` 14 passed；`test_raw_store_io` all passed；`test_replay` ALL PASSED；`testbench` Round-trip OK

### 2026-07-01 更新 README.md 和 AGENTS.md

- **变更类型**: docs
- **涉及文件**: README.md, AGENTS.md, docs/devlog.md
- **变更内容**:
  1. README.md: 修复 compile_commands.json 生成方式描述（不再自动生成，改为 `xmake project -k compile_commands`）；添加 `xmake testbench` 运行方式；添加构建产物说明（libsysal.a/so）；添加版本管理说明；修正 v0.0.1 范围描述（NVML→nvidia-smi 命令输出）
  2. AGENTS.md: 修复 compile_commands.json 描述；添加 `xmake testbench` 命令
- **原因**: 文档与实际构建配置脱节，compile_commands 生成方式已变更但文档未更新
- **验证**: 文审

### 2026-07-01 重写 testbench.cpp 消除冗余并修正输出格式

- **变更类型**: tests / refactor
- **涉及文件**: tests/testbench.cpp, docs/devlog.md
- **变更内容**:
  1. 新增 `format_frequency()` 辅助函数，按量级自动选择 GHz/MHz/kHz/Hz；CPU 频率输出从 "2200000 MHz" 修正为 "2 GHz"
  2. Section 2 移除 `Container: yes/no` 行（容器信息仅在 Section 10 展示）
  3. Section 3 移除 `find_package`/`find_core`/`find_logical_cpu`/`logical_cpus_of_package`/`cores_of_package`/`visible_logical_cpus` 的无意义断言与输出
  4. Section 5/6/8 移除 `find()` 断言与输出（已在单元测试覆盖）
  5. Section 6 网卡接口名与状态分两行输出（`Name:` / `State:`），替代单行 `eth0 (UP)`
  6. Section 8 PCI 仅展示前 5 个设备 + 剩余计数，优先显示 `device_name`（非空时）
  7. Section 11 可见性改为仅输出计数摘要，不再逐项罗列 52 个 CPU/接口
  8. Section 13 移除与 Section 1 重复的 `collect_duration`/`succeeded_collectors`/`failed_collectors` 字段，仅保留 Section 1 未展示的 `sysal_version`/`requested_flags`/`warnings`
  9. Section 15 标注请求域与非请求域，突出 partial 语义差异
  10. 文件从 844 行缩减至 784 行
- **原因**: 评审发现 13 处冗余/格式问题——跨节重复数据、无意义断言、频率单位错误、PCI 设备过多、可见性节刷屏
- **验证**: `xmake -r` 构建成功零 warning；`xmake testbench` 运行通过全部断言；clang-tidy 零告警；LSP 无诊断

### 2026-07-01 R2a: if-has(flags) 链重构为表驱动分派

- **变更类型**: src / refactor
- **涉及文件**: src/pipeline/pipeline.cpp, src/reader/linux/sysfs.cpp, src/reader/linux/procfs.cpp, docs/devlog.md
- **变更内容**:
  1. `pipeline.cpp`：9 个 `if(has(flags, Collect::Xxx))` 块替换为 `ParserDispatch` 表 + 循环。每个条目含 `Collect flag` 和函数指针（无捕获 lambda 通过 `+` 运算符转换为 `void(*)(ParseResult&, const RawStore&, vector<string>&)`），避免 `std::function` 开销
  2. `sysfs.cpp`：6 个 `if(has(flags, Collect::Xxx))` 块替换为 `ReaderDispatch` 表 + 循环。所有读取函数签名统一为 `void(*)(RawStore&)`，直接使用函数指针
  3. `procfs.cpp`：保留显式 if-has 块，添加注释说明原因——Cpu→Platform、Pci→Network、Software→Accelerator 三个跨域依赖使扁平表驱动分派不可行
- **原因**: 消除重复的 if-has 模式，提升可读性和可维护性；新增域只需在分发表添加一行
- **验证**: `xmake -r` 构建成功零 warning；`xmake run test_collect` 和 `xmake run test_replay` 全部通过

### 2026-07-01 抽象 resource.cpp 重复代码为模板辅助函数

- **变更类型**: src / refactor
- **涉及文件**: src/model/resource.cpp, docs/devlog.md
- **变更内容**:
  1. 新增匿名命名空间内两个模板辅助函数：`find_by_member`（按成员指针查找）和 `filter_by`（按谓词过滤）
  2. 将 6 个 `find_by_id` 模式函数（`Cpu::find_package`、`Cpu::find_core`、`Cpu::find_logical_cpu`、`Accelerators::find`、`Network::find`、`Pci::find`）替换为 `find_by_member` 单行调用
  3. 将 6 个 `filter_by_predicate` 模式函数（`Cpu::logical_cpus_of_package`、`Cpu::logical_cpus_of_core`、`Cpu::cores_of_package`、`Cpu::visible_logical_cpus`、`Accelerators::visible`、`Network::visible`）替换为 `filter_by` 单行调用
  4. 保留 `Accelerators::by_kind`/`gpus`/`npus`/`fpgas` 不变（已为委托链）
  5. 新增 `#include <type_traits>` 用于 `std::remove_reference_t`
  6. 文件从 200 行缩减至 144 行
- **原因**: resource.cpp ~80% 为重复的线性扫描模式，模板化消除冗余、提升可维护性
- **验证**: `xmake -r` 构建通过零 warning，`xmake run test_model` 全部通过，clang-tidy 零告警

### 2026-07-01 修复 4 个库 Bug（R1a/R1c/R1d/R1e）

- **变更类型**: src / fix
- **涉及文件**: src/parser/accelerator.cpp, src/reader/linux/file_utils.hpp, src/parser/platform.cpp, src/reader/linux/procfs.cpp, include/sysal/types/enums.hpp, tests/testbench.cpp, docs/devlog.md
- **变更内容**:
  1. R1a: `accelerator.cpp` — nvidia-smi CSV 字段 2/3 顺序修正：field[2] 为 memory.total，field[3] 为 pci.bus_id（原代码反了）
  2. R1c: `file_utils.hpp` — `read_command()` 追加 `2>/dev/null` 重定向 stderr，避免命令不存在时错误信息泄漏到终端
  3. R1d: `platform.cpp` — `kernel.version` 从 `/proc/version` 中提取 `#` 开头的构建版本标签（如 `#101-Ubuntu`），而非复制 `kernel.release`
  4. R1e: `enums.hpp` 新增 `ProcHostname` 枚举值；`procfs.cpp` 采集 `/proc/sys/kernel/hostname`；`platform.cpp` 解析 hostname 替换原警告占位
  5. `testbench.cpp` — switch 补充 `ProcHostname` 分支（-Wswitch 要求完整覆盖）
- **原因**: 4 个独立 Bug 修复，详见任务描述
- **验证**: `xmake -r` 构建通过，零 warning

### 2026-07-01 修复 R1 测试回归 + ProcHostname 枚举顺序

- **变更类型**: tests / fix
- **涉及文件**: tests/test_parse_accelerator.cpp, tests/test_parse_platform.cpp, include/sysal/types/enums.hpp, docs/devlog.md
- **变更内容**:
  1. `test_parse_accelerator.cpp` — 3 个测试 fixture 的 CSV 列顺序更新为 nvidia-smi 实际输出顺序 `index,name,memory.total,pci.bus_id,driver_version`
  2. `test_parse_platform.cpp` — `kernel.version` 断言从 `"5.15.0-91-generic"` 改为 `"#101-Ubuntu"`
  3. `enums.hpp` — `ProcHostname` 从 `ProcOneCgroup` 后移到枚举末尾，避免插入中间导致后续枚举值数字偏移、破坏已有 JSON fixture 的序列化兼容性
- **原因**: R1a/R1d 修复改变了行为，测试断言需同步更新；ProcHostname 插入中间导致 dev_machine.json fixture 中所有 source 数字偏移
- **验证**: 全部 20 个测试通过

### 2026-07-01 修复容器检测误报（bare metal 上报 Docker）

- **变更类型**: src / fix
- **涉及文件**: include/sysal/model/raw_store.hpp, src/model/raw_store.cpp, src/parser/execution.cpp, docs/devlog.md
- **变更内容**:
  1. `raw_store.hpp`：新增 `has_success(RawSource)` 声明，仅当记录状态为 `CollectStatus::Success` 时返回 true
  2. `raw_store.cpp`：实现 `has_success()`，使用 `std::ranges::any_of` 过滤 `source` 和 `status == Success`
  3. `execution.cpp`：`detect_container()` 中 `raw.has(RawSource::RootDockerenv)` 改为 `raw.has_success(RawSource::RootDockerenv)`
- **原因**: `has()` 不检查 `CollectStatus`，当 `/.dockerenv` 不存在时 reader 插入 `NotCollected` 记录，`has()` 仍返回 true，导致 bare metal 机器误报 Docker 容器
- **验证**: `xmake -r` 构建成功，零 warning

### 2026-07-01 集中版本管理 + GitHub Release v0.0.1

- **变更类型**: src / tests / build
- **涉及文件**: include/sysal/version.hpp (新增), src/pipeline/pipeline.cpp, src/serialization/serialize.cpp, tests/testbench.cpp, tests/test_collect.cpp, tests/test_serialization.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. 新建 `include/sysal/version.hpp`：`inline constexpr` 定义 `VERSION_MAJOR=0`、`VERSION_MINOR=0`、`VERSION_PATCH=1`、`VERSION_STRING="0.0.1"`
  2. `pipeline.cpp`：`meta.sysal_version = "0.0.1"` 改为 `sysal::VERSION_STRING`
  3. `serialize.cpp`：版本兼容性检查 `starts_with("0.0.")` 改为用 `VERSION_MAJOR`/`VERSION_MINOR` 动态构建前缀
  4. `testbench.cpp`、`test_collect.cpp`、`test_serialization.cpp`：版本断言改为 `sysal::VERSION_STRING`
  5. `xmake.lua`：添加 `set_version("0.0.1")`
- **原因**: 版本号从单点硬编码改为集中管理，便于版本升级时统一修改
- **验证**: `xmake -r` 构建成功；`xmake run test_collect` 通过；`xmake run test_serialization` 14 passed

### 2026-07-01 整理 xmake.lua 结构

- **变更类型**: build / refactor
- **涉及文件**: xmake.lua, docs/devlog.md
- **变更内容**:
  1. 删除 `after_build` 手写 compile_commands.json 生成（18 行），check.sh 已有 `xmake project -k compile_commands` 自动生成
  2. 合并 `test_target` 和 `test_target_shared` 为一个函数，通过 `link_shared` 布尔参数控制链接静态/动态库
  3. 19 个 test_target 调用改为循环 + 列表
  4. 四层视觉结构：全局配置 / 库 / 测试 / task，用 `==========` 分隔
  5. git hooks `on_load` 加注释说明为何挂在 `sysal_static` 上
  6. 静态库 `set_targetdir` 加注释说明 workaround 原因
- **原因**: 原 xmake.lua 有重复代码、职责混乱（项目级逻辑挂在 target 上）、冗余的手写 JSON 生成
- **验证**: `xmake -r` 构建成功；`xmake testbench` 运行正常；`xmake project -k compile_commands build` 生成 compile_commands.json

### 2026-07-01 添加 xmake testbench task + 移除 testbench 内部 include 路径

- **变更类型**: build / chore
- **涉及文件**: xmake.lua, docs/devlog.md
- **变更内容**:
  1. xmake.lua: 添加 `task("testbench")` 插件任务，`xmake testbench` 一键编译并运行 testbench，终端输出完整可见
  2. xmake.lua: `test_target_shared` 移除 `add_includedirs("src")`，testbench 不再能访问 `src/` 内部头文件，强制封装边界
- **原因**: testbench 需要便捷的运行方式查看终端输出；作为链接动态库的外部程序，testbench 不应访问库内部头文件
- **验证**: `xmake -r` 构建成功；`xmake testbench` 运行正常，终端输出 17 section 完整内容

### 2026-07-01 拆分静态库/动态库目标 + 重写 testbench 完整能力测试

- **变更类型**: build / tests / chore
- **涉及文件**: xmake.lua, utils/check.sh, tests/testbench.cpp, docs/devlog.md
- **变更内容**:
  1. xmake.lua: 原 `sysal` 目标拆分为 `sysal_static`（static）和 `sysal_shared`（shared），共享同一源文件列表 `SYSAL_SOURCES`；`set_basename("sysal")` 使输出为 `libsysal.a` / `libsysal.so`；静态库通过 `set_targetdir` 放到 `static/` 子目录避免链接器优先选择 `.so`
  2. xmake.lua: 新增 `test_target_shared` 辅助函数，testbench 改用 `test_target_shared` 链接 `sysal_shared`；19 个单元测试仍链接 `sysal_static`
  3. xmake.lua: `on_load`（git hooks）和 `after_build`（compile_commands.json）保留在 `sysal_static` 目标上
  4. utils/check.sh: 测试目标发现 grep 正则从 `test_target\("` 扩展为 `(?:test_target|test_target_shared)\("`，确保 testbench 被发现
  5. tests/testbench.cpp: 从 323 行打印 demo 重写为 856 行完整能力测试，覆盖 17 个 section：全量采集、Platform/CPU/Memory/Accelerators/Network/Storage/PCI/Software/Execution 各域详细输出与查询方法测试、可见性筛选、Raw Store、Warnings/Meta、JSON 序列化往返、部分采集、Refresh、错误处理（空 RawStore 抛 SysalError）
- **原因**: 作为库需要同时提供静态库和动态库两种构建目标；testbench 需完整测试所有公共 API 能力
- **验证**: `xmake -r` 构建成功（`libsysal.a` 在 `static/` 子目录 + `libsysal.so` 在主目录 + 全部测试）；`ldd test_types` 确认无动态依赖（静态链接）；`ldd testbench` 确认链接 `libsysal.so`；`utils/check.sh` 全部 4 项通过（20 测试）

### 2026-07-01 F3: 一致性修复（/// @file 头、include 风格、testbench 断言、NDEBUG 防护）

- **变更类型**: src / tests / build / chore
- **涉及文件**: src/parser/cpu.cpp, src/parser/execution.cpp, src/parser/platform.cpp, src/parser/accelerator.cpp, src/parser/memory.cpp, src/serialization/serialize.cpp, tests/test_types.cpp, tests/test_model.cpp, tests/test_raw_store_io.cpp, tests/test_reader.cpp, tests/test_replay.cpp, tests/testbench.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. F3a: 5 个 parser .cpp 文件添加 `/// @file` + `/// @brief` + `/// @details` Doxygen 头（cpu/execution/platform/accelerator/memory）
  2. F3b: 7 个文件中的 `#include <sysal/...>` 统一改为 `#include "sysal/..."`（serialize.cpp + 6 个测试文件）
  3. F3c: testbench.cpp 添加 `#include <cassert>` 和 9 个基本断言（collect 后 7 个 + refresh 后 2 个），使其既是 demo 也是有效测试
  4. F3e: xmake.lua 的 `test_target` helper 添加 `add_cxxflags("-UNDEBUG", {force = true})`，防止测试被 `-DNDEBUG` 编译时 `assert()` 被禁用
- **原因**: 代码一致性和测试质量问题——部分 .cpp 缺少 Doxygen 头、include 风格不统一、testbench 无断言、assert 可能被禁用
- **验证**: `utils/check.sh` 全部 4 项通过（clang-format + clang-tidy + build + tests）

### 2026-07-01 F2c: 4 个 parser 空数据时添加 warning

- **变更类型**: src / fix
- **涉及文件**: src/parser/network.cpp, src/parser/pci.cpp, src/parser/storage.cpp, src/parser/accelerator.cpp, docs/devlog.md
- **变更内容**: network/pci/storage/accelerator 4 个 parser 在源数据为空时静默返回 nullopt，现添加 warning 消息（`"parse_network: 缺少 SysfsNet 数据"` 等），与 cpu/memory/platform parser 保持一致
- **原因**: 4 个 parser 与 cpu/memory/platform 行为不一致，缺少 warning 不利于调试
- **验证**: `utils/check.sh` 全部 4 项通过

### 2026-07-01 F2: 修复 parse_uint/parse_hex 部分消费 + collect() 全失败抛异常

- **变更类型**: src / fix
- **涉及文件**: src/parser/parse_utils.cpp, tests/test_parse_utils.cpp, src/pipeline/pipeline.cpp, tests/test_collect.cpp, docs/devlog.md
- **变更内容**:
  1. `parse_uint` / `parse_hex` 新增 `ptr != trimmed.data() + trimmed.size()` 检查：`from_chars` 部分消费时返回 `nullopt`，拒绝 `"123abc"` / `"ffxyz"` 等输入
  2. `run_replay` 在 `record_collector_status` 之后检查全部请求采集器失败时抛出 `SysalError(ErrorKind::CollectionFailed, ...)`
  3. 新增测试：parse_uint/parse_hex 部分消费拒绝、空 RawStore 调用 collect_from_raw 抛 SysalError
- **原因**: `from_chars` 不要求消费全部输入，原实现静默接受部分消费导致错误值；设计文档要求全部采集器失败时抛 SysalError 但原实现未实现
- **验证**: `utils/check.sh` 全部通过（clang-format + clang-tidy + build + tests）

### 2026-07-01 F1a: 修复 read_cpufreq 忽略 package_id

- **变更类型**: src / fix
- **涉及文件**: src/parser/cpu.cpp, tests/test_parse_cpu.cpp, docs/devlog.md
- **变更内容**:
  1. 新增 `extract_cpu_number_from_path` 辅助函数：从 sysfs 路径（如 `cpu/cpu0/cpufreq/base_frequency`）中提取 CPU 编号
  2. `read_cpufreq` 签名修改：移除 `[[maybe_unused]]`，新增 `package_cpu_ids` 参数（该封装包含的逻辑 CPU 编号集合）
  3. `read_cpufreq` 逻辑修复：遍历 sysfs 记录时提取 CPU 编号，仅处理属于该封装的 CPU 记录。原实现对所有封装都取第一个 `base_frequency` 和 `scaling_max_freq`，导致多 socket 系统所有封装获得 cpu0 的频率
  4. 调用点修改：从 `entries` 收集每个封装的逻辑 CPU 编号集合，传入 `read_cpufreq`
  5. 新增测试 7：2 封装各 2 CPU，频率不同（package 0: 2400/3500 MHz，package 1: 1800/2900 MHz），断言各封装频率正确且互不相同
- **原因**: 多 socket 系统中所有封装获得相同频率（cpu0 的），违反设计意图
- **验证**: `utils/check.sh` 全部 4 项通过；`xmake run test_parse_cpu` 7 个测试全部通过

### 2026-07-01 F1b+F1c: 修复交叉校验同义反复 + 实现冲突解决框架

- **变更类型**: src / fix
- **涉及文件**: src/resolver/resolve.cpp, tests/test_resolve.cpp, docs/devlog.md
- **变更内容**:
  1. `cross_check_cpu_visibility` 重写：原实现比较 `visible_to_current_process` 与 `visible_logical_cpu_ids`（同义反复，compute 已从后者设置前者，永远一致）。新实现检测两类问题：(a) 幻影 ID——`visible_logical_cpu_ids` 引用模型中不存在的 CPU，格式 `[visibility_mismatch] cpu_N: in_visible_logical_cpu_ids but cpu does not exist in model`；(b) 约束提示——cpuset 限制可见 CPU 数量，格式 `[constraint] cpu visibility restricted: N total, M visible`
  2. `cross_check_accelerator_visibility` 同理重写：检测加速器幻影 ID 和环境变量约束提示
  3. 新增 `TrustLevel` 枚举（Backend=0 > Sysfs=1 > Procfs=2 > Command=3 > Inferred=4）和 `resolve_conflict` 辅助函数：当两来源值不同时，高信任（低数值）来源胜出，追加 `[conflict]` 格式警告；值相同时无冲突。v0.0.1 无多来源字段故未调用，框架已就绪
  4. `resolve()` 中冲突解决占位注释更新为框架就绪说明
  5. 测试 7 重写为幻影 ID 检测（CPU 99 不存在于模型）；新增测试 8（cpuset 约束提示 4 total 2 visible）；新增测试 9（加速器幻影 ID 检测）
- **原因**: F1b——原交叉校验是同义反复，无法发现真实问题（幻影 ID）；F1c——冲突解决仅有注释占位，需实现可测试的 helper
- **验证**: `utils/check.sh` 全部 4 项通过（clang-format + clang-tidy + build + tests）；`xmake run test_resolve` 9 个测试全部通过

### 2026-07-01 新增代码质量评审方法与首份评审报告

- **变更类型**: docs
- **涉及文件**: docs/code_quality_review_method.md, docs/quality_reports/v001_initial_rewrite.md
- **变更内容**:
  1. 新增 `docs/code_quality_review_method.md`：定义可复用的多维度代码质量评审方法（9 个维度、权重分配、Oracle agent 分批执行流程、prompt 模板、汇总报告格式）
  2. 新增 `docs/quality_reports/v001_initial_rewrite.md`：v0.0.1 重写后的首份质量评审报告（加权总分 8/10，9 维度详评，11 个关键问题按 P0/P1/P2 排序）
- **原因**: 建立可复用的代码质量评价体系，便于后续开发周期重复执行；记录首次评审基线供后续对比
- **验证**: 方法文档与报告文档分离存放；评审由 5 个 Oracle agent 分 3 批并行执行

### 2026-06-30 Phase 10: 测试基础设施与重写收尾

- **变更类型**: src / build / docs
- **涉及文件**: tests/test_replay.cpp, tests/testbench.cpp, tests/fixtures/, xmake.lua, docs/issues.md, docs/devlog.md
- **变更内容**:
  1. `tests/test_replay.cpp`：raw replay 测试。首次运行时自动采集当前机器原始数据并保存到 `tests/fixtures/dev_machine.json`；后续运行加载 fixture 执行 Parser→Resolver 回放管线，验证域不变量（CPU 非空、内存 > 0、平台信息非空、网络/PCI 非空、执行上下文有效、元数据完整），并与实时采集对比关键指标（CPU 数量、内存总量）
  2. `tests/testbench.cpp`：全量 API 演示。采集当前机器全部系统信息，以格式化文本输出各子域（Platform/CPU/Memory/Accelerator/Network/PCI/Storage/Software/Execution），演示 JSON 序列化（pretty print）与 refresh 功能。PCI 地址使用十六进制零填充格式（`%04x:%02x:%02x.%x`），速率输出 Mbps，内存输出 GiB/MiB
  3. `tests/fixtures/`：fixture 目录，存放 raw replay 测试的原始数据快照
  4. `xmake.lua`：新增 `test_replay` 和 `testbench` 两个测试目标
  5. `docs/issues.md`：更新全部 18 个已知问题的状态——14 个已通过重写修复、2 个部分修复（B-2 NVMe symlink 链、D-8 软件栈大面积空白）、2 个已移除（D-3 TopologyInfo、D-5 PciRelation）。移除旧类型名引用（TopologyInfo、Diagnostics、PlatformInfo）
  6. `docs/devlog.md`：本条目
- **原因**: Phase 10 重写收尾——创建最终测试基础设施，更新文档反映重写成果
- **验证**: `xmake -r` 构建成功，`xmake run test_replay` 全部通过，`xmake run testbench` 输出正常，`utils/check.sh` 全部通过

### 2026-06-30 重写总结（Phase 1–10）

sysal v0.0.1 重写完成。10 个阶段的核心变更：

| Phase | 内容 | 关键产出 |
|-------|------|----------|
| 1 | 设计文档重写 | 19 个设计文档，反映新架构 |
| 2 | 数据模型 | 11 个 model 头文件，System/SystemInfo/Collect |
| 3 | 类型系统 | StrongId/NamedString/ScalarUnit 模板，消除重复 |
| 4 | JSON 引擎 | 纯 JSON 解析/发射，无 sysal 耦合 |
| 5 | RawStore 序列化 | save/load_raw_store，测试基础设施 |
| 6 | Linux Reader | procfs + sysfs 采集器，C-1 修复 |
| 7 | 9 个域 Parser | platform/cpu/memory/accelerator/storage/pci/network/software/execution |
| 8 | Resolver + Pipeline + System API | collect/refresh/collect_from_raw，可见性计算 |
| 9 | System JSON 序列化 | to_json/from_json，版本兼容 |
| 10 | 测试基础设施 | test_replay + testbench + fixture + 文档更新 |

修复的已知问题：A-1/A-2/A-3、B-1/B-3、C-1/C-2/C-3/C-4、D-1/D-2/D-4/D-6/D-7、E-1/E-2/E-3、F-1/F-2（共 16 个）。部分修复：B-2、D-8。已移除：D-3、D-5。

### 2026-06-30 Phase 9: System JSON 序列化（to_json / from_json）

- **变更类型**: src / build
- **涉及文件**: include/sysal/serialization/serialization.hpp, src/serialization/serialize.cpp, tests/test_serialization.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. `serialization.hpp`：新增 SerializationOptions 结构体（pretty_print、include_raw、include_meta）、to_json / from_json 自由函数声明
  2. `serialize.cpp`：在已有 RawStore 序列化基础上扩展 System↔JSON 实现。to_json 序列化 info（9 个子域）、meta（可选）、warnings、raw（可选）；from_json 解析全部字段并做版本兼容性检查（0.0.x）。为每个子结构体实现独立的 to/from 辅助函数，处理强类型（StrongId::value()、NamedString::value、ScalarUnit::value、PciAddress 对象、enum static_cast）、可选字段（firmware、virtualization、cuda、rocm、level_zero、mpi、rdma、container 等）
  3. `test_serialization.cpp`：7 个测试（round-trip 往返、include_raw=false 无 raw 键、include_raw=true 有 raw 键、include_meta=false 无 meta 键、include_meta=true 有 meta 键、版本不兼容抛 SysalError、兼容版本正常解析）
  4. `xmake.lua`：新增 test_serialization 测试目标
- **原因**: Phase 9 实现——System JSON 序列化与反序列化，支持 raw replay 测试管线
- **验证**: xmake -r 成功，xmake run test_serialization 14 项全部通过

### 2026-06-30 Phase 8: Resolver、Pipeline、System::collect/refresh

- **变更类型**: src / build
- **涉及文件**: src/resolver/resolve.hpp, src/resolver/resolve.cpp, src/pipeline/pipeline.hpp, src/pipeline/pipeline.cpp, src/api/system.cpp, include/sysal/test/replay.hpp, tests/test_resolve.cpp, tests/test_collect.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. `resolve.hpp` + `resolve.cpp`：实现 `resolve(ParseResult, warnings) → SystemInfo`。移动 ParseResult 各 optional 域到 SystemInfo（缺省默认构造）；计算 CPU 可见性（cpuset 约束）、加速器可见性（CUDA_VISIBLE_DEVICES）、网络接口可见性（v0.0.1 全部可见）；交叉校验资源级 visible_to_current_process 与 ExecutionContext 便利索引的一致性，不一致时追加 `[visibility_mismatch]` 警告；冲突解决框架就绪（v0.0.1 大多单来源，格式 `[conflict] <field>: <src1>=<val>, <src2>=<val>, adopted=<src>`）
  2. `pipeline.hpp` + `pipeline.cpp`：实现 `run_pipeline(flags, warnings)` 和 `run_replay(raw, flags, warnings)`。run_pipeline 执行 Reader→Parser→Resolver 完整管线；run_replay 从已有 RawStore 执行 Parser→Resolver 回放管线；按域调用 9 个解析器；记录成功/失败采集器；构建 SnapshotMeta（collect_time、sysal_version="0.0.1"、collect_duration、requested_flags）；后端 init/shutdown 生命周期占位；实现 `sysal::test::collect_from_raw` 公共接口
  3. `system.cpp`：实现 `System::collect(flags)` 委托 `run_pipeline`，`System::refresh()` 用 `meta.requested_flags` 重新采集并移动赋值
  4. `replay.hpp`：新增 `collect_from_raw(raw, flags)` 声明
  5. `test_resolve.cpp`：7 个测试（CPU 可见性 cpuset 约束、CPU 无约束全可见、加速器可见性、加速器无约束、网络接口全可见、缺失域默认构造、交叉校验一致性）
  6. `test_collect.cpp`：3 个测试（collect 冒烟、refresh 保持一致、元数据正确）
  7. `xmake.lua`：新增 test_resolve、test_collect 两个测试目标
- **原因**: Phase 8 核心实现——Resolver 冲突解决与可见性计算、Pipeline 管线编排、System 公共 API
- **验证**: `utils/check.sh` 全部 4 项检查通过（clang-format + clang-tidy + build + tests）

### 2026-06-30 Phase 7: Software、Execution 域解析器

- **变更类型**: src / build
- **涉及文件**: src/parser/software.hpp, src/parser/software.cpp, src/parser/execution.hpp, src/parser/execution.cpp, tests/test_parse_software.cpp, tests/test_parse_execution.cpp, xmake.lua
- **变更内容**: 新增 software 和 execution 两个域解析器。software 解析 NVIDIA 驱动版本（nvidia-smi）和 CUDA 版本（nvcc --version），构建 SoftwareStack。execution 解析 /proc/self/status（进程、权限、cpuset）、/proc/self/cgroup（cgroup v1/v2）、环境变量、容器检测，构建 ExecutionContext。两个解析器均遵循 parse_<domain>(const RawStore&, warnings) → optional<T> 接口，不调用任何 syscall。
- **原因**: Phase 7 最后两个域解析器，完成全部 10 个域的解析器实现
- **验证**: xmake -r 成功，xmake run test_parse_software 和 test_parse_execution 通过，utils/check.sh 全部 4 项检查通过

### 2026-06-30 Phase 7: Accelerator、Storage 域解析器

- **变更类型**: src / build
- **涉及文件**: src/parser/accelerator.hpp, src/parser/accelerator.cpp, src/parser/storage.hpp, src/parser/storage.cpp, tests/test_parse_accelerator.cpp, tests/test_parse_storage.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. `accelerator.hpp` + `accelerator.cpp`：实现 `parse_accelerator(RawStore&, warnings)` → `std::optional<Accelerators>`。解析 NvidiaSmi CSV 输出（index/name/pci.bus_id/memory.total）→ AcceleratorDevice 列表，支持 MiB/GiB/KiB 单位转换，SysfsPci numa_node 查找（D-4 修正）
  2. `storage.hpp` + `storage.cpp`：实现 `parse_storage(RawStore&, warnings)` → `std::optional<Storage>`。解析 SysfsBlock 记录（按设备名分组，size×512→容量），设备名前缀推断 StorageKind（nvme→Nvme, sd→Sata），B-2 修正：PCI 地址暂缺并发出警告
  3. `test_parse_accelerator.cpp`：4 个测试（2 GPU 解析、NUMA 查找、空数据 nullopt、GiB 单位）
  4. `test_parse_storage.cpp`：4 个测试（nvme+sda 解析、空数据 nullopt、Other 类型推断、B-2 警告）
  5. `xmake.lua`：新增 test_parse_accelerator、test_parse_storage 两个测试目标
- **原因**: Phase 7 域解析器实现，accelerator 和 storage 两个子域
- **验证**: `utils/check.sh` 全部通过（clang-format + clang-tidy + build + tests）

### 2026-06-30 Phase 7: PCI、Network 域解析器

- **变更类型**: src / build
- **涉及文件**: src/parser/pci.hpp, src/parser/pci.cpp, src/parser/network.hpp, src/parser/network.cpp, tests/test_parse_pci.cpp, tests/test_parse_network.cpp, xmake.lua
- **变更内容**: 新增 PCI 和 Network 域解析器，从 RawStore 中解析 Pci/Network 结构体；PCI 解析器从 SysfsPci 记录提取地址、厂商、设备名、类别、NUMA 节点；Network 解析器从 SysfsNet 记录提取接口名、MAC、链路状态、速率；新增对应测试；xmake.lua 添加 test_parse_pci 和 test_parse_network 目标
- **原因**: Phase 7 域解析器扩展，覆盖 PCI 和 Network 子系统
- **验证**: `utils/check.sh` 全量通过（clang-format + clang-tidy + build + tests）

### 2026-06-30 Phase 7: Platform、CPU、Memory 域解析器

- **变更类型**: src / build
- **涉及文件**: src/parser/platform.hpp, src/parser/platform.cpp, src/parser/cpu.hpp, src/parser/cpu.cpp, src/parser/memory.hpp, src/parser/memory.cpp, tests/test_parse_platform.cpp, tests/test_parse_cpu.cpp, tests/test_parse_memory.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. `platform.hpp` + `platform.cpp`：实现 `parse_platform(RawStore&, warnings)` → `std::optional<Platform>`。解析 EtcOsRelease（os-release 键值对）→ Os，ProcVersion（内核发行号、编译时间）→ Kernel，Uname（架构名称、位宽、字节序）→ Architecture，SysfsDmi（BIOS 厂商/版本/日期、产品名/厂商/序列号）→ Firmware + Host，ProcOneCgroup + RootDockerenv（Docker/KVM 虚拟化检测）→ Virtualization
  2. `cpu.hpp` + `cpu.cpp`：实现 `parse_cpu(RawStore&, warnings)` → `std::optional<Cpu>`。解析 ProcCpuInfo（逐行解析，空行分隔条目）→ CpuPackage/CpuCore/LogicalCpu 拓扑，flags → IsaExtension 列表，SysfsCpu（cpufreq base_frequency/scaling_max_freq）→ 频率，SysfsNuma（cpulist 范围解析）→ NumaNode 映射
  3. `memory.hpp` + `memory.cpp`：实现 `parse_memory(RawStore&, warnings)` → `std::optional<Memory>`。解析 ProcMemInfo（MemTotal/MemAvailable kB→bytes）→ 总量/可用，SysfsNuma（nodeN/meminfo）→ NumaMemory 列表
  4. 三个测试文件：手造 RawStore 载荷，断言结构化字段（6 组平台测试、6 组 CPU 测试、5 组内存测试）
  5. xmake.lua 新增 test_parse_platform、test_parse_cpu、test_parse_memory 三个测试目标
- **原因**: Phase 7 要求实现 Parser 层，将 RawStore 原始证据解析为强类型模型
- **验证**: `utils/check.sh` 全部通过（clang-format + clang-tidy + build + 9 tests）

### 2026-06-30 Phase 6: Linux procfs 与 sysfs 采集器

- **变更类型**: src / build
- **涉及文件**: src/reader/linux/file_utils.hpp, src/reader/linux/procfs.hpp, src/reader/linux/procfs.cpp, src/reader/linux/sysfs.hpp, src/reader/linux/sysfs.cpp, tests/test_reader.cpp, xmake.lua, docs/devlog.md
- **变更内容**:
  1. `file_utils.hpp`：header-only 工具库，提供 `read_file`（读取文件全部内容）、`read_command`（popen 执行命令并读取标准输出）、`file_exists`（检查文件是否存在）、`add_record`（向 RawStore 添加 RawRecord 便利函数）
  2. `procfs.hpp` + `procfs.cpp`：实现 `read_procfs(RawStore&, Collect flags)`，按 Collect 位掩码采集：Platform 域（/proc/cpuinfo、/proc/version、/etc/os-release、uname -m、/.dockerenv）、Memory 域（/proc/meminfo）、Accelerator 域（nvidia-smi、nvcc）、Network 域（lspci）、Storage 域（lsblk）、Pci 域（lspci 补充）、Software 域（nvidia-smi/nvcc 补充）、Execution 域（/proc/self/cgroup、/proc/self/status、/proc/1/cgroup、7 个环境变量）。跨域共享来源（如 ProcCpuInfo）仅采集一次
  3. `sysfs.hpp` + `sysfs.cpp`：实现 `read_sysfs(RawStore&, Collect flags)`，按 Collect 位掩码采集：Cpu 域（遍历 /sys/devices/system/cpu/cpuN，读取 topology/physical_package_id、topology/core_id、online、cpufreq/base_frequency、cpufreq/scaling_max_freq）、Memory 域（遍历 /sys/devices/system/node/nodeN，读取 cpulist、meminfo）、Network 域（遍历 /sys/class/net，读取 address、operstate、speed）、Pci 域（遍历 /sys/bus/pci/devices，读取 vendor、device、class、numa_node）、Storage 域（遍历 /sys/block，读取 size、device/model）、Platform 域（/sys/class/dmi/id 下 6 个 DMI 文件）
  4. `tests/test_reader.cpp`：file_utils 测试（read_file 成功/失败、file_exists）、procfs 采集测试（10 个 RawSource 断言）、sysfs 采集测试（6 个 RawSource 断言）、合并采集测试（记录数与采集状态验证）
  5. xmake.lua 新增 `test_target("test_reader", "tests/test_reader.cpp")`
- **原因**: Phase 6 重写计划要求实现 Reader 层，将所有原始数据采集到 RawStore，修复 C-1 bug（parser 不直接调用 syscall，而是从 Reader 采集的 /proc/self/status 中解析 PID/UID/GID）
- **验证**: `utils/check.sh` 全部 4 项通过（clang-format + clang-tidy + build + tests）；`xmake run test_reader` 全部断言通过

### 2026-06-30 Phase 5: RawStore JSON 序列化与 save/load 测试基础设施

- **变更类型**: src / build
- **涉及文件**: include/sysal/test/replay.hpp, src/serialization/serialize.cpp, tests/test_raw_store_io.cpp, xmake.lua, src/reader/linux/file_utils.hpp, src/reader/linux/procfs.cpp, src/reader/linux/sysfs.cpp, docs/devlog.md
- **变更内容**:
  1. `include/sysal/test/replay.hpp`：声明 `load_raw_store` 和 `save_raw_store`（`collect_from_raw` 留待 Phase 8）
  2. `src/serialization/serialize.cpp`：实现 RawStore ↔ JSON 序列化。JSON 格式为顶层对象含 `records` 数组，每条记录含 `source`（RawSource 整数值）、`path_or_command`（字符串）、`payload`（字符串）、`status`（CollectStatus 整数值）、`collected_at`（epoch 毫秒）。实现 `save_raw_store`（写 JSON 到文件）和 `load_raw_store`（读文件并解析为 RawStore），失败时抛 `SysalError`
  3. `tests/test_raw_store_io.cpp`：4 组测试——往返一致性（3 条不同来源记录 save→load→比较）、加载不存在文件抛 SysalError(FileNotFound)、加载畸形 JSON 抛 SysalError(DeserializationError)、空 RawStore 往返
  4. xmake.lua 新增 `test_target("test_raw_store_io", "tests/test_raw_store_io.cpp")`
  5. 修复预存文件的 clang-format 和 clang-tidy 问题（file_utils.hpp/procfs.cpp/sysfs.cpp 格式化，sysfs.cpp 中 `auto dir = entry.path()` 改为 `const auto& dir = entry.path()` 消除 unnecessary-copy-initialization 警告）
- **原因**: Phase 5 重写计划要求实现 RawStore JSON 序列化与 save/load 测试基础设施，为 raw replay 测试策略提供持久化能力
- **验证**: `utils/check.sh` 全部 4 项通过（clang-format + clang-tidy + build + tests）；`xmake run test_raw_store_io` 4/4 通过

### 2026-06-30 Phase 2: 数据模型头文件与实现

- **变更类型**: src
- **涉及文件**: include/sysal/model/platform.hpp, include/sysal/model/cpu.hpp, include/sysal/model/memory.hpp, include/sysal/model/accelerator.hpp, include/sysal/model/network.hpp, include/sysal/model/storage.hpp, include/sysal/model/pci.hpp, include/sysal/model/software.hpp, include/sysal/model/execution.hpp, include/sysal/model/raw_store.hpp, include/sysal/model/system_info.hpp, include/sysal/core/system.hpp, include/sysal/core/sysal.hpp, src/model/raw_store.cpp, src/model/resource.cpp, tests/test_model.cpp, xmake.lua
- **变更内容**:
  1. 创建 11 个 model 头文件：platform/cpu/memory/accelerator/network/storage/pci/software/execution/raw_store/system_info
  2. 创建 2 个 core 头文件：system.hpp（System 类）、sysal.hpp（总入口）
  3. 实现 raw_store.cpp：get_all/get/has/count 四个查询方法
  4. 实现 resource.cpp：Cpu 7 个查询方法、Accelerators 6 个查询方法、Network 2 个查询方法、Pci 1 个查询方法
  5. 创建 test_model.cpp：Cpu/Accelerators/Pci/RawStore 查询方法测试
  6. xmake.lua 新增 test_model 测试目标
- **原因**: Phase 2 重写计划要求创建全部数据模型头文件与实现文件
- **验证**: `utils/check.sh` 全部通过（clang-format + clang-tidy + build + tests）

### 2026-06-30 Phase 4: 提取纯 JSON 引擎

- **变更类型**: src
- **涉及文件**: src/serialization/json.hpp, tests/test_json.cpp, xmake.lua
- **变更内容**:
  1. 从旧 `src/detail/json.hpp`（git HEAD~2）提取纯 JSON 部分，创建 `src/serialization/json.hpp`
  2. 移除所有 sysal 头文件依赖，移除 `raw_store_to_json`/`raw_store_from_json` 函数
  3. 用 `JsonError`（继承 `std::exception`）替代 `SysalError`/`Expected` 错误处理
  4. 新增 `dump_json()` 函数，将 `JsonVal` 发射为 JSON 文本
  5. 保留 `escape_string`、`JsonObj`、`JsonArr`、`JsonVal`、`JsonParser`、`parse_json`、`time_point_to_ms`、`ms_to_time_point`
  6. 创建 `tests/test_json.cpp`：基本类型、字符串转义、容器、嵌套、dump、往返一致性、escape_string、时间工具、错误处理、构建器共 10 组测试
  7. xmake.lua：`test_target` 辅助函数增加 `add_includedirs("src")`，新增 `test_target("test_json", "tests/test_json.cpp")`
- **原因**: Phase 4 重写计划要求将手写 JSON 引擎从旧代码中提取为独立纯 JSON 库，消除 sysal 类型耦合
- **验证**: `utils/check.sh` 全部通过（clang-format + clang-tidy + build + tests）

### 2026-06-30 修复设计文档遗漏（pipeline + raw_store）

- **变更类型**: docs
- **涉及文件**: docs/design/architecture/pipeline.md, docs/design/data_model/raw_store.md
- **变更内容**:
  1. pipeline.md：parser 目录列表补上遗漏的 `platform.hpp / platform.cpp`（ParseResult 有 9 个 optional 字段含 platform，但目录列表只列了 8 个 parser）
  2. raw_store.md：`RawSource` 枚举从 11 个值扩展到 23 个，按来源分组（procfs / sysfs / 文件命令 / 环境变量 / 未来后端），补上 C-1 修复（parser 不直接调用 syscall）和数据空白修复（D-1/D-2/D-4/D-6/D-7）所需的全部来源
- **原因**: 重写计划审计阶段发现 pipeline.md 的 parser 目录遗漏 platform parser；raw_store.md 的 RawSource 枚举缺少实现所需的来源值（ProcSelfStatus/ProcSelfCgroup/ProcOneCgroup 用于 execution parser 的 C-1 修复，SysfsNuma 用于 CPU NUMA 归属和内存 NUMA 分布，SysfsDmi 用于固件信息，EtcOsRelease/Uname 用于 platform parser，Environment 用于环境变量采集，RootDockerenv 用于容器检测，Nvcc 用于 CUDA 版本检测，SysfsBlock 用于存储设备）
- **验证**: 文档审查，确认 pipeline.md parser 目录列表与 ParseResult 的 9 个字段一一对应；raw_store.md 的 RawSource 枚举覆盖全部 parser 所需的来源

### 2026-06-30 从 resource_info.md 拆分 6 个子系统设计文档

- **变更类型**: docs
- **涉及文件**: docs/design/data_model/cpu.md, docs/design/data_model/memory.md, docs/design/data_model/accelerator.md, docs/design/data_model/network.md, docs/design/data_model/pci.md, docs/design/data_model/storage.md
- **变更内容**:
  1. 将原 resource_info.md 设计文档按子系统拆分为 6 个独立文件，每个文件聚焦一个子系统
  2. 按新命名规则重命名聚合类型：`CpuSubsystem` → `Cpu`、`MemorySubsystem` → `Memory`、`AcceleratorSubsystem` → `Accelerators`、`NetworkSubsystem` → `Network`、`StorageSubsystem` → `Storage`、`PciSubsystem` → `Pci`、`NumaMemoryInfo` → `NumaMemory`
  3. cpu.md：包含 StrongId 类型定义（CpuPackageId/CpuCoreId/LogicalCpuId）、CpuPackage/CpuCore/LogicalCpu/Cpu 结构体及便利查询方法；说明 `LogicalCpu::package_id` 反范式化与 `numa_node` 直接从 sysfs 读取
  4. memory.md：Memory 与 NumaMemory 结构体
  5. accelerator.md：AcceleratorKind 枚举、AcceleratorDevice 与 Accelerators 结构体；说明 `devices` 是数据真源、便利方法为非持有型过滤、`nearest_numa_node` 直接从 sysfs 读取
  6. network.md：NetworkInterface 与 Network 结构体
  7. pci.md：PciDevice 与 Pci 结构体；说明 Pci 是设备清单、`numa_node` 直接从 sysfs 读取
  8. storage.md：StorageDevice 与 Storage 结构体；说明 v0.0.1 仅提供基本设备清单
  9. 不包含 `ResourceInfo` 聚合类型（已移除，SystemInfo 直接包含各子系统）
- **原因**: 拆分单一大文档为聚焦的子系统文档，便于维护；统一去除 `Info`/`Subsystem` 后缀，简化类型命名
- **验证**: 文档审查，确认无 `ResourceInfo`/`CpuSubsystem`/`MemorySubsystem`/`AcceleratorSubsystem`/`NetworkSubsystem`/`StorageSubsystem`/`PciSubsystem`/`NumaMemoryInfo` 等带后缀类型名残留；代码块类型名与 ids.hpp/enums.hpp/resource_info.hpp 一致

### 2026-06-30 重写 7 个数据模型设计文档以反映新架构

- **变更类型**: docs
- **涉及文件**: docs/design/data_model/system_snapshot.md, docs/design/data_model/platform_info.md, docs/design/data_model/resource_info.md, docs/design/data_model/software_stack_info.md, docs/design/data_model/execution_context.md, docs/design/data_model/raw_store.md, docs/design/data_model/diagnostics.md
- **变更内容**:
  1. system_snapshot.md：`SystemSnapshot` 改为 `std::vector<std::string> warnings` 替代 `Diagnostics`，`ResourceInfo` 不含 topology；新增 `System` 类说明（对象持有模式，构造时抛 `SysalError`，构造后不可变）；`SnapshotMeta::requested_spec` 改为 `requested_flags`（类型 `Collect`）
  2. platform_info.md：保持内容，确认中文
  3. resource_info.md：`ResourceInfo` 移除 `TopologyInfo topology` 字段；保留 `CpuCore::numa_node`、`LogicalCpu::numa_node`、`AcceleratorDevice::nearest_numa_node`、`PciDevice::numa_node`，注释改为"从 sysfs 直接读取"；移除所有 TopologyInfo 引用与"PciSubsystem 是清单 / TopologyInfo 是关系图"对比
  4. software_stack_info.md：保持内容，确认中文
  5. execution_context.md：保持内容，确认无拓扑引用
  6. raw_store.md：`RawSource` 移除 `HwlocXml`，保留其余值；说明 `RawStore` 在 `SystemSnapshot` 中可选，通过 `Collect::Raw` 启用（替代 `CollectSpec::with_raw()`）
  7. diagnostics.md：标题改为 Warnings（警告信息），移除 `Severity`/`ConflictDetail`/`Diagnostic`/`Diagnostics` 结构体，改为描述 `std::vector<std::string> warnings`，保留简化示例
- **原因**: 反映 sysal 架构变更（System 类替代 collect/collect_or_throw、Collect 位掩码替代 CollectSpec、移除 Expected/SysalError 抛出、移除 TopologyInfo 等拓扑结构、Diagnostics 简化为 warnings、RawSource 移除 HwlocXml）
- **验证**: 文档审查，确认无 TopologyInfo/NumaRelation/PciRelation/DeviceLocality/Diagnostics/ConflictDetail/Severity/Expected/CollectSpec/HwlocXml 残留

### 2026-06-30 重写 5 个架构与规则设计文档以反映新架构

- **变更类型**: docs
- **涉及文件**: docs/design/architecture/pipeline.md, docs/design/architecture/backend_strategy.md, docs/design/rules/strong_typing.md, docs/design/rules/conflict_resolution.md, docs/design/rules/thread_safety.md
- **变更内容**:
  1. pipeline.md：管线改为 `Reader → RawStore → Parser → ParseResult → Resolver → System`；`ParsedFacts` 重命名为 `ParseResult`，字段改为公共类型，移除 topology 字段；更新源码布局为新命名（`cpu.hpp`、`procfs.hpp`、`resolve.hpp`、`system.cpp` 等），移除 `backend/` 目录
  2. backend_strategy.md：移除 hwloc 与 Topology 行，更新后端策略为 procfs+sysfs+PCI / NVML / ROCm SMI / Level Zero / ibverbs，说明 NUMA 设备级 `numa_node` 从 sysfs 读取
  3. strong_typing.md：保持不变（无 TopologyInfo 引用），确认中文
  4. conflict_resolution.md：来源信任顺序移除 hwloc，移除 `ConflictDetail`/`Diagnostics`，冲突改为字符串形式记录到 `System::warnings()`
  5. thread_safety.md：更新为对象持有模式（`System::collect()`、`System` 对象、`System::refresh()`、内部组件），实现约束改为 System 对象构造后只提供 const 访问
- **原因**: 反映 sysal 架构变更（System 类替代双入口 API、Collect 位掩码、移除 Expected/Diagnostics/TopologyInfo/hwloc、ParseResult 重命名、文件命名调整）
- **验证**: 文档审查，确认无拓扑/hwloc/Diagnostics/Expected/CollectSpec 及旧文件命名残留

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

### 2026-06-27 为 parser 第一组文件添加中文 Doxygen 注释

- **变更类型**: docs
- **涉及文件**: src/parser/cpu_parser.hpp, src/parser/cpu_parser.cpp, src/parser/memory_parser.hpp, src/parser/memory_parser.cpp, src/parser/network_parser.hpp, src/parser/network_parser.cpp, src/parser/platform_parser.hpp, src/parser/platform_parser.cpp, src/parser/pci_parser.hpp, src/parser/pci_parser.cpp
- **变更内容**:
  1. 每个文件开头添加文件级 Doxygen 注释块（@file/@brief/@details）
  2. 每个函数（含匿名命名空间内函数）添加 Doxygen 头注释（@brief/@param/@return）
  3. 匿名命名空间内结构体（CpuInfoEntry）添加注释说明
  4. 函数内部关键逻辑添加行内中文注释：CPU 拓扑去重与回退策略、NUMA meminfo 键前缀匹配、网络接口速率单位转换、/proc/version 字段位置解析等
  5. 代码逻辑、缩进、换行、#include、命名空间结构均未改动
- **原因**: 为解析器层补充中文 API 文档，便于团队理解解析逻辑
- **验证**: `utils/check.sh` 4/4 通过（clang-format / clang-tidy / build / tests）；10 个文件均通过 clang-format 校验

### 2026-06-27 为 reader/linux 与 detail 目录添加中文 Doxygen 注释

- **变更类型**: docs
- **涉及文件**: src/reader/linux/file_utils.hpp, src/reader/linux/procfs_reader.hpp, src/reader/linux/procfs_reader.cpp, src/reader/linux/sysfs_reader.hpp, src/reader/linux/sysfs_reader.cpp, src/detail/pipeline.hpp, src/detail/pipeline.cpp, src/detail/algorithm.hpp, src/detail/json.hpp
- **变更内容**:
  1. 9 个文件均添加文件级 `@file`/`@brief`/`@details` 注释块
  2. 每个函数（含匿名命名空间内函数、模板函数、类成员函数）添加 `@brief`/`@param`/`@return` Doxygen 注释
  3. 函数内部关键逻辑添加行内中文注释：procfs 各采集分支用途、sysfs 目录遍历与属性读取、pipeline 解析-组装流程、JSON 序列化/反序列化数据格式与转义/UTF-8 编码逻辑
  4. json.hpp 说明 RawStore JSON 格式（records 数组、source/status 整数编码、collected_at epoch 毫秒）
  5. 代码逻辑、#include、命名空间结构均未改动；仅添加注释，未删除已有注释
- **原因**: 为采集层与基础设施层补充中文 API 文档，与解析器层注释风格保持一致
- **验证**: `utils/check.sh` 4/4 通过（clang-format / clang-tidy / build / tests）

### 2026-06-27 为 include/sysal 公共头文件添加中文 Doxygen 注释

- **变更类型**: docs
- **涉及文件**: include/sysal/ 下全部 21 个 .hpp 文件（enums.hpp, error.hpp, expected.hpp, units.hpp, strong_id.hpp, value_types.hpp, ids.hpp, platform_info.hpp, raw_store.hpp, diagnostics.hpp, collect_spec.hpp, collect.hpp, resource_info.hpp, topology_info.hpp, snapshot_meta.hpp, software_stack_info.hpp, execution_context_info.hpp, serialization.hpp, system_snapshot.hpp, sysal.hpp, test/replay.hpp）
- **变更内容**:
  1. 每个文件开头添加文件级 `@file`/`@brief`/`@details` 注释块
  2. 每个结构体/类添加 `@brief`/`@details` Doxygen 注释
  3. 每个结构体关键成员变量添加 `///<` 行尾注释
  4. 每个函数声明/方法添加 `@brief`/`@param`/`@return`/`@throws` 注释（简单 getter/setter 仅 @brief）
  5. 所有枚举类型与枚举值添加注释；类型别名（using）添加行尾注释
  6. 保留 strong_id.hpp 已有英文 Doxygen 注释并补充其余注释
  7. 代码逻辑、缩进、#include、#pragma once、命名空间结构均未改动
- **原因**: 为公共 API 头文件补充完整中文 Doxygen 文档，提升 API 可发现性与可维护性
- **验证**: `utils/check.sh` 4/4 通过（clang-format / clang-tidy / build / tests）

### 2026-06-27 为核心文件、解析器、后端、公共 API、测试添加中文 Doxygen 注释

- **变更类型**: docs
- **涉及文件**: src/raw_store.cpp, src/serialization.cpp, src/resource_info.cpp, src/resolver/resolver.hpp, src/resolver/resolver.cpp, src/backend/hwloc_backend.hpp, src/backend/hwloc_backend.cpp, src/public_api/collect.cpp, src/test/replay.cpp, docs/devlog.md
- **变更内容**:
  1. 为 9 个核心文件添加文件级 Doxygen 注释块（@file/@brief/@details）
  2. 为每个函数（含匿名命名空间内函数与条件编译分支内的函数）添加 Doxygen 头注释（@brief/@param/@return/@throws/@details）
  3. 函数内部关键逻辑添加行内中文注释：
     - resolver.cpp 说明从 ParsedFacts 构建 SystemSnapshot 的流程（子字段移动、默认可见性、cpuset 覆盖、可见资源汇总）
     - serialization.cpp 每个 to_json_* 函数说明 JSON 输出格式（对象键名、数组结构、可选字段、枚举/数值/布尔/PCI 地址的表示方式）
     - hwloc_backend.cpp 说明 NUMA 节点查找、PCI 设备遍历与拓扑加载流程
   4. 代码逻辑、#include、#pragma once、命名空间结构均未改动；未删除已有注释（将 resolver.cpp 中原有英文行内注释译为中文以统一注释语言）
- **原因**: 为库的核心实现层补充中文 API 文档，与已有头文件/解析器层 Doxygen 风格保持一致，便于维护与生成文档
- **验证**: `utils/check.sh` 4/4 通过（clang-format / clang-tidy / build / tests）

### 2026-06-27 数据模型设计文档中文化

- **变更类型**: docs
- **涉及文件**: docs/design/data_model/software_stack_info.md, docs/design/data_model/execution_context.md, docs/design/data_model/raw_store.md, docs/design/data_model/diagnostics.md
- **变更内容**: 将 4 个数据模型设计文档从英文重写为中文。翻译正文、表格内容、代码块内注释；保持代码逻辑、字符串字面量、类型名/函数名/变量名、Markdown 结构不变；保留 examples 示例块（英文输出文本）原样以体现真实采集结果；专有名词（CUDA、ROCm、MPI、RDMA、UCX、NVML、Level Zero 等）保持英文。
- **原因**: 统一设计文档语言为中文，与项目其余文档及代码注释风格一致
- **验证**: 逐文件对照原文确认结构、代码块、表格、列表完整保留

### 2026-06-30 重写测试相关设计文档以反映架构变更

- **变更类型**: docs
- **涉及文件**: docs/design/testing/serialization.md, docs/design/testing/raw_replay.md
- **变更内容**: 重写 2 个测试相关设计文档为中文，反映新架构：`System` 类替代 `collect()`/`collect_or_throw()`，`Collect` 位掩码枚举替代 `CollectSpec`，移除 `Expected<T,E>`，所有接口失败时抛 `SysalError`。serialization.md 接口改为 `to_json(const System&, ...)` / `from_json(std::string_view)`，强调非侵入式自由函数、独立头文件、手写 JSON 序列化、`SnapshotMeta::sysal_version` 兼容性检查。raw_replay.md 接口改为 `load_raw_store` / `collect_from_raw` / `save_raw_store` 直接返回值；工作流使用 `System::collect` + `Collect::Raw`；管线对比更新为 `Reader → RawStore → Parser → ParseResult → Resolver → System`；保留 Fixture 布局。
- **原因**: sysal 公共 API 架构变更（异常替代 Expected、System/Collect 替代旧 API），测试相关设计文档需同步
- **验证**: 逐文件确认无 Expected/CollectSpec/collect_or_throw 残留，C++ 代码块语法正确，专有名词与类型名保持英文

### 2026-06-30 重写 4 个核心设计文档反映架构变更

- **变更类型**: docs
- **涉及文件**: docs/design/index.md, docs/design/overview.md, docs/design/public_api.md, docs/design/roadmap.md
- **变更内容**: 用 Write 覆盖重写 4 个核心设计文档以反映新架构：
  1. index.md：目录结构移除 topology_info.md / diagnostics.md（diagnostics 改为 warnings 内嵌 System），文档索引表移除 topology_info 行、更新各文档描述（overview 移除拓扑、public_api 改为 System 类、pipeline 改为新文件命名 ParseResult、backend_strategy 移除 hwloc）、阅读顺序更新
  2. overview.md：管线图改为 `Reader → RawStore → Parser → ParseResult → Resolver → System`，架构总结移除拓扑构建、改为 System 对象，API 调用示例改为 `System::collect()`
  3. public_api.md：完整重写，描述 System 类（对象持有模式）、Collect 位掩码枚举、operator| 与 has()、basic/full 预设、System 完整接口、使用示例（链式 flags + 预设）、失败抛 SysalError 而非 Expected、公共 API 不暴露内部 reader/parser/backend
  4. roadmap.md：v0.0.1 范围改为 System::collect/refresh + Collect bitmask、Core model 移除 Topology 与 Diagnostics（改为 warnings）、内部管线 ParseResult、移除 hwloc 后端、非目标添加"拓扑信息（已有 hwloc 等成熟库）"、未来扩展说明缓存已内置（System 对象即缓存）、拓扑作为独立可选模块
- **原因**: API 重构（移除 collect/collect_or_throw 双入口、Expected、CollectSpec builder，引入 System 类 + Collect bitmask）、移除 hwloc 拓扑后端、ParsedFacts 重命名为 ParseResult、Diagnostics 简化为 warnings，设计文档需同步反映
- **验证**: 逐文件确认无 CollectSpec / collect() / collect_or_throw() / Expected / TopologyInfo / hwloc 残留描述；代码块 C++ 语法正确；专有名词与类型名保持英文

### 2026-07-02 迁移 nlohmann/json 从 vendor 到 xrepo 包管理

- **变更类型**: build / chore
- **涉及文件**: xmake.lua, utils/check.sh, .gitignore, docs/design/testing/serialization.md, third_party/nlohmann/（删除）
- **变更内容**:
  1. xmake.lua：移除 `add_includedirs("third_party")`，改为 `add_requires("nlohmann_json")` + `add_packages("nlohmann_json")`，通过 xrepo 管理依赖（版本 v3.12.0）
  2. 删除 `third_party/nlohmann/` 整个目录（之前手动 vendor 的 v3.11.2 源码）
  3. utils/check.sh：移除 pre-commit hook 中对 `third_party/*` 路径的排除过滤（不再需要）
  4. .gitignore：添加 `.xrepo/` 排除规则
  5. docs/design/testing/serialization.md：更新 JSON 库描述从 "vendored 在 third_party/nlohmann/" 改为 "通过 xmake add_requires 从 xrepo 管理依赖"
- **原因**: 手动 vendor 第三方库不可持续——版本更新靠手动拷贝、仓库体积膨胀、无锁定机制。改用 xmake 原生包管理器 xrepo，版本声明在 xmake.lua 中，构建时自动拉取缓存
- **验证**: `xmake -r` 构建成功（0 errors 0 warnings）；全部 18 个测试通过（528 assertions）；`grep -r third_party` 确认代码中无残留引用

### 2026-07-05 移动 sysal.hpp 到顶层 include 目录

- **变更类型**: refactor
- **涉及文件**: include/sysal/core/sysal.hpp → include/sysal/sysal.hpp, examples/sysal_info.cpp
- **变更内容**: 将 `include/sysal/core/sysal.hpp` 移动到 `include/sysal/sysal.hpp`，同步更新 `examples/sysal_info.cpp` 中的 include 路径
- **原因**: `sysal.hpp` 是库的入口头文件，应放在 `include/sysal/` 顶层而非 `core/` 子目录，使 include 路径更直观（`#include "sysal/sysal.hpp"`）
- **验证**: `xmake -r` 构建成功；`xmake run sysal_info` 正常输出

### 2026-07-05 Network IP 地址 + PCI 地址 + Storage df -Th + Memory DIMM 详情

- **变更类型**: feat / src
- **涉及文件**: include/sysal/types/enums.hpp, include/sysal/model/storage.hpp, include/sysal/model/memory.hpp, src/reader/linux/procfs.cpp, src/reader/linux/sysfs.cpp, src/parser/network.cpp, src/parser/storage.cpp, src/parser/memory.cpp, src/serialization/serialize.cpp, examples/sysal_info.cpp, tests/unit/test_parse_network.cpp, tests/unit/test_parse_storage.cpp, tests/unit/test_parse_memory.cpp
- **变更内容**:
  1. **Network IP 地址**: procfs.cpp 新增 `read_ifaddrs()` 使用 `getifaddrs()` 采集接口 IP（IPv4+IPv6），序列化为 `IfAddrs` RawSource；network.cpp 解析填充 `NetworkInterface.addresses`
  2. **Network PCI 地址**: sysfs.cpp 新增 `read_symlink()` 读取 `/sys/class/net/<ifname>/device` 符号链接目标；network.cpp 提取最后一PathComponent 用 `parse_pci_address()` 解析填入 `NetworkInterface.pci_address`
  3. **Storage df -Th**: procfs.cpp 将 `lsblk` 替换为 `df -Th`；storage.hpp 新增 `mount_point` 和 `fs_type` 字段；storage.cpp 解析 df 输出按设备名与 sysfs 合并（精确匹配 + 分区前缀匹配）
  4. **Memory DIMM**: enums.hpp 新增 `Udevadm` 和 `SysfsEdac` RawSource；procfs.cpp 执行 `udevadm info -e`；sysfs.cpp 读取 `/sys/devices/system/edac/mc/mcN/dimmM/`；memory.hpp 新增 `DimmInfo` 结构体（13 字段）+ Memory 加 `dimms`/`dimm_count`/`populated_dimms`；memory.cpp 以 udevadm 为主解析 `MEMORY_DEVICE_N_FIELD` 条目，EDAC 为辅 fallback；序列化和 sysal_info 同步更新
- **原因**: Network 缺 IP 和 PCI 地址、Storage 缺挂载点和文件系统类型、Memory 缺 DIMM 级别硬件信息（频率/代数/厂商/型号）
- **验证**: `xmake -r` 构建成功（0 warnings）；全部 18 个测试通过（678 assertions）；sysal_info 正确显示 27 DIMM 槽位/11 已用、Samsung DDR4-3200 32GiB、网络 IP 和 PCI 地址、存储挂载点和文件系统类型

### 2026-07-05 修复设计文档缺口 + 新增数据源选择原则文档

- **变更类型**: docs
- **涉及文件**: docs/design/data_model/raw_store.md, docs/design/data_model/storage.md, docs/design/data_model/memory.md, docs/design/architecture/pipeline.md, docs/design/roadmap.md, docs/design/overview.md, docs/design/testing/serialization.md, docs/design/index.md, docs/design/architecture/data_source_guideline.md (新增), docs/naming_rules.md (删除), README.md, AGENTS.md, docs/devlog.md
- **变更内容**:
  1. `raw_store.md`: RawSource 枚举补充 IfAddrs、DfTh、Udevadm、SysfsEdac 四个新值，按来源分组排列
  2. `storage.md`: StorageDevice 结构体补充 mount_point 和 fs_type 字段；设计说明更新为 v0.0.4 新增 df -Th 采集
  3. `memory.md`: 新增完整 DimmInfo 结构体（13 字段）；Memory 结构体补充 dimms、dimm_count、populated_dimms 字段；设计说明增加双源策略（udevadm 主 + EDAC sysfs 备）
  4. `pipeline.md`: 源码布局中 sysal.hpp 移至顶层（不在 core/ 下）；Reader 描述补充 getifaddrs、udevadm、df -Th、EDAC sysfs；新增外部依赖小节说明 nlohmann/json 通过 xrepo 管理
  5. `roadmap.md`: 新增 v0.0.4 实现范围章节（网络 IP/PCI 地址、存储挂载点、DIMM 详情、syscall 优化、nlohmann/json 迁移、sysal.hpp 位置调整）
  6. `README.md`: 新增 v0.0.4 范围章节；开发环境表格补充 nlohmann_json 依赖说明
  7. `overview.md`: Reader 描述扩展为包含 syscall、命令执行、EDAC sysfs 等数据来源
  8. `serialization.md`: JSON 结构示例补充 memory.dimms、storage.devices[].mount_point/fs_type 字段
  9. `data_source_guideline.md` (新增): 数据源选择原则文档，覆盖核心原则（syscall > 文件读取 > 命令执行）、理由、决策表、例外规则、新增数据源检查清单
  10. `index.md`: 目录结构和文档索引表新增 data_source_guideline.md 引用；roadmap 描述更新为 v0.0.3/v0.0.4
  11. `naming_rules.md` (删除): 文件内容来自 blas_benchmark 项目，与 sysal 无关
  12. `AGENTS.md`: 命名规则引用从 `docs/命名规则.md` 更新为 `docs/design/rules/strong_typing.md`
- **原因**: 7 个 commit 新增功能后设计文档未同步更新；缺少数据源选择原则的正式文档；naming_rules.md 包含错误项目内容
- **验证**: 文档审查，确认所有设计文档与当前代码状态一致；`docs/naming_rules.md` 已删除；`data_source_guideline.md` 格式与现有设计文档一致

### 2026-07-05 v0.0.4 代码质量评审 + O2 优化

- **变更类型**: build / docs
- **涉及文件**: xmake.lua, docs/code_quality_review_method.md, docs/quality_reports/v004_review.md, docs/devlog.md
- **变更内容**:
  1. xmake.lua：全局编译选项添加 `-O2` 优化
  2. code_quality_review_method.md：新增评审前提、维度调整指南、版本对比模板、评审后行动、分批灵活性说明
  3. v004_review.md：v0.0.4 代码质量评审报告（9 维度，5 Oracle agent，加权总分 8/10）
- **原因**: 版本冻结前收尾——添加生产级编译优化，改进评审方法文档，执行完整代码质量评审
- **验证**: `xmake -r` 构建成功（0 warnings）；18/18 测试通过（678 assertions）；评审报告完成，7 项 P1 + 13 项 P2 问题记录
