# 冲突解决策略

## 来源信任优先级（高 → 低）

```txt
1. Dedicated backends (NVML, ibverbs)              — direct hardware query
2. sysfs                                           — kernel structured data
3. procfs                                          — kernel text data
4. Command output (lspci, nvidia-smi)              — may have version skew
5. Inference / defaults                            — last resort
```

## 按冲突类别的规则

| 类别 | 规则 | 示例 |
|---|---|---|
| **数量** | 最高信任来源胜出 | GPU 显存：NVML 96GB 对比 sysfs 98GB → NVML |
| **可见性** | 执行上下文胜出 | CPU：procfs 192 对比 cpuset 32 → cpuset 32 |
| **标识** | 最高信任胜出，不匹配时记录警告 | GPU 名称：NVML "H20" 对比 lspci "GA140" → NVML + 警告 |
| **状态** | 最新采集时间胜出 | 链路状态：sysfs 对比 ethtool → 最新 |

所有冲突以警告字符串形式记录在 `System` 对象的 `warnings` 成员中。
