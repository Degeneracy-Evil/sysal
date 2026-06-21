# Conflict Resolution Strategy

## Source Trust Order (high → low)

```txt
1. Dedicated backends (NVML, hwloc, ibverbs)   — direct hardware query
2. sysfs                                        — kernel structured data
3. procfs                                       — kernel text data
4. Command output (lspci, nvidia-smi)           — may have version skew
5. Inference / defaults                         — last resort
```

## Rules by Conflict Category

| Category | Rule | Example |
|---|---|---|
| **Quantity** | Highest trust source wins | GPU memory: NVML 96GB vs sysfs 98GB → NVML |
| **Visibility** | Execution context wins | CPU: procfs 192 vs cpuset 32 → cpuset 32 |
| **Topology** | Prefer hwloc, fallback sysfs | NUMA affinity: hwloc vs sysfs → hwloc |
| **Identity** | Highest trust wins, log warning on mismatch | GPU name: NVML "H20" vs lspci "GA140" → NVML + warning |
| **State** | Most recent collection time wins | Link state: sysfs vs ethtool → latest |

All conflicts are recorded in `Diagnostics` with `ConflictDetail`.
