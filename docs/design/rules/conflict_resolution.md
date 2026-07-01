# 冲突解决策略

## 当前实现（v0.0.3）

v0.0.3 的 Resolver 职责有限：将 `ParseResult` 中的字段移动到 `SystemInfo`，
计算可见性，交叉校验便利索引。当前版本是**单来源**的——每个字段只来自一个
Parser，不存在多来源冲突。

### Resolver 管线

```
ParseResult → resolve() → SystemInfo
```

`resolve()` 函数（`src/resolver/resolve.cpp`）做三件事：

1. **移动字段**：将 `ParseResult` 中各子域（platform、cpu、memory 等）的
   `std::optional` 值移动到 `SystemInfo` 对应成员。若某子域为 `nullopt`，
   保留默认构造值。
2. **计算可见性**：以 `ExecutionContext` 中的便利索引
   （`visible_logical_cpu_ids`、`visible_accelerator_ids`）为依据，
   设置各资源子域的 `visible_to_current_process` 字段。
3. **交叉校验**：检测便利索引中的幻影 ID（引用了模型中不存在的资源），
   记录警告。同时记录约束提示（如 cpuset 限制了可见 CPU 数量）。

### 可见性计算

| 子域 | 依据 | 说明 |
|------|------|------|
| CPU | `visible_logical_cpu_ids` | cpuset 约束，空列表=全部可见 |
| Accelerator | `visible_accelerator_ids` | CUDA_VISIBLE_DEVICES 等约束 |
| Network | 全部可见 | v0.0.3 中网络命名空间检测推迟 |

### 交叉校验

Resolver 检测两类问题并写入 `warnings`：

- **幻影 ID**：便利索引引用了模型中不存在的资源 ID
- **约束提示**：可见资源数量少于模型中的总量（信息性）

## 未来方向

当 NVML、ibverbs 等后端加入后，同一字段可能来自多个来源
（如 GPU 显存：NVML 报告 96GB，sysfs 报告 98GB），届时需要多来源冲突解决。

计划中的冲突解决框架：

- **来源信任优先级**：专用后端（NVML、ibverbs）> sysfs > procfs > 命令输出 > 推断值
- **冲突类别**：数量冲突、可见性冲突、标识冲突、状态冲突、归属冲突
- **记录格式**：`[conflict] <字段名>: <高优先级来源>=<值>, <低优先级来源>=<值>, adopted=<采用的来源>`

当前版本不实现此框架。当多来源场景出现时，再按需引入。