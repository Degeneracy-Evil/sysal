# 冲突解决策略

当多个数据来源对同一字段提供不同值时，sysal 需要确定性地选择一个并记录冲突。

## 冲突的前提条件

冲突仅在**多个来源提供同一字段**时发生。例如：
- GPU 显存：NVML 报告 96GB，sysfs 报告 98GB → 冲突
- GPU 名称：NVML 报告 "H20"，lspci 报告 "GA140" → 冲突

如果某字段只有一个来源，则不存在冲突。如果某来源未提供该字段，也不构成冲突。

## 来源信任优先级（高 → 低）

```txt
1. 专用后端（NVML、ibverbs）     — 直接查询硬件，最可信
2. sysfs                         — 内核结构化数据
3. procfs                        — 内核文本数据
4. 命令输出（lspci、nvidia-smi） — 可能有版本偏差
5. 推断 / 默认值                  — 最后手段
```

## 按冲突类别的规则

| 类别 | 规则 | 示例 |
|---|---|---|
| **数量** | 最高信任来源胜出 | GPU 显存：NVML 96GB 对比 sysfs 98GB → 采用 NVML |
| **可见性** | 执行上下文胜出 | CPU：procfs 192 对比 cpuset 32 → 采用 cpuset 32 |
| **标识** | 最高信任胜出，不匹配时记录警告 | GPU 名称：NVML "H20" 对比 lspci "GA140" → 采用 NVML + 警告 |
| **状态** | 最新采集时间胜出 | 链路状态：sysfs 对比 ethtool → 采用最新采集的 |
| **归属** | 专用后端胜出，其次 sysfs | NUMA 归属：NVML 对比 sysfs → 采用 NVML；sysfs 对比 procfs → 采用 sysfs |

## 冲突记录格式

所有冲突以警告字符串形式记录在 `System` 对象的 `warnings` 成员中。
格式为：`[conflict] <字段名>: <高优先级来源>=<值>, <低优先级来源>=<值>, adopted=<采用的来源>`

示例：

```txt
[conflict] gpu_memory_size: NVML=96GB, sysfs=98GB, adopted=NVML
[conflict] gpu_name: NVML=H20, lspci=GA140, adopted=NVML
```

非冲突的普通警告（如某来源不可用）不需要此格式，直接描述问题即可。
