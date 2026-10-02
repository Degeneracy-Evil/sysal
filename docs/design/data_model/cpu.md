# CPU

公共定义见 `include/sysal/model/cpu.hpp`。拓扑由 `CpuPackage → CpuCore → LogicalCpu`
的强类型 ID 关联，不假定每个插槽、核心或线程具有相同能力。

## 拓扑与在线状态

- `CpuPackage`：厂商、型号、已描述的核心/线程数、基础频率和硬件最高频率。
- `CpuCore`：所属封装、已描述的线程数、NUMA 归属。
- `LogicalCpu`：核心/封装/NUMA 归属、进程可见性、可选在线状态和处理器标识。
- `Cpu.present_cpu_ids` / `online_cpu_ids` 保存内核明确报告的集合；缺失为未知，
  与成功读取的空集合不同。
- sysfs 有效拓扑编号优先于 `/proc/cpuinfo`。离线线程仅在内核明确提供封装和核心
  编号时加入完整拓扑，其身份不从其他线程复制；present 集合可能更完整。
  不把拓扑列表长度解释成芯片的理论线程上限。
- `smt_active` 是内核报告的活动状态；`smt_control` 是控制状态原值，
  `on` 不代表此刻有核心正在使用 SMT。离线线程不作为进程可见资源。

## 处理器标识

每个逻辑 CPU 的 `identification` 独立保存 `CpuIdentification`：

| 字段 | 含义 |
| --- | --- |
| family / model / stepping | x86 家族、型号编号、步进 |
| implementer / part / variant | ARM 的原始对应编号，保持与 x86 的语义区别 |
| revision / architecture | ARM 修订和 `/proc/cpuinfo` 的架构描述 |
| microcode | 系统报告的微码版本原值 |
| features | 当前线程完整内核能力标识，排序、去重 |

缺失编号保持未知。`isa_extensions` 保留既有枚举接口，完整能力查询使用线程的
`features`；能力标识还包含其他 CPU/内核能力，不等于芯片全部物理能力。
不从型号字符串推导制程、微架构代号、IPC 或内存控制器理论规格。

## 频率策略

`frequency_policies` 保存每个 `CpuFrequencyPolicy`。频率使用强类型 `Frequency`，
单位 Hz；解析 sysfs kHz 时检查数值和乘法溢出。

| 字段 | sysfs 来源 |
| --- | --- |
| related_cpus / affected_cpus | 同名文件：前者含所属在线与离线线程，后者是当前受影响线程 |
| base_frequency | base_frequency |
| hardware_min_frequency / hardware_max_frequency | cpuinfo_min_freq / cpuinfo_max_freq |
| scaling_min_frequency / scaling_max_frequency | scaling_min_freq / scaling_max_freq |
| scaling_current_frequency | scaling_cur_freq：可能是请求值或驱动报告值 |
| hardware_current_frequency | cpuinfo_cur_freq：仅在后端提供时设置 |
| driver / governor / energy_performance_preference | scaling_driver / scaling_governor / energy_performance_preference |

`index` 对应 `policyN` 的 N；旧内核只提供 `cpuN/cpufreq` 时使用采样 CPU 编号，
按明确的 related CPU 集合合并重复条目。RawStore 保留实际路径。
两种目录均不存在时不创建虚构策略。

封装的 `base_frequency` 仅在已取得的值一致时设置，`max_frequency` 为已取得的
硬件最高频率最大值，策略上限不替代硬件最高频率。
整机 `governor` 在已报告值一致时返回该值，不同值返回 `mixed`。
`boost_enabled` 仅在通用 `cpufreq/boost` 明确提供 0/1 时设置。

## 缓存

`CpuCache` 保存层级、类型、单实例容量、相联度、缓存行大小与采样 CPU。
`cache_id`、`sets`、`shared_cpus` 对应缓存 ID、组数和共享线程集合。
共享集合、层级、类型、ID 和已取得属性相同的记录合并为一个实例。
没有共享集合时保留采样记录，不按相同容量猜测共享关系。
SystemCard 提供实例统计，在 CPU 详情展示每个缓存的共享集合。

## 采集与兼容

Reader 读取 `/proc/cpuinfo`、sysfs 的 topology、online、cache、CPUFreq、SMT 信息。
Parser / Resolver 只消费 RawStore，不读取当前机器。
可选字段缺失不导致整个 CPU 采集失败；旧 JSON 中缺少新增字段时保持未知或空列表。
温度和 NUMA 归属继续使用既有接口。
