# v0.0.2 R5 功能增强计划

## 背景

R1-R4-R6 已完成并提交。R5 原为 v0.0.2 的最后一批改动。

## 状态：全部已完成

R5a-R5d 在 v0.0.3~v0.0.5 期间逐步完成，计划文件未同步更新。现核实如下：

| 项 | 内容 | 完成状态 | 验证 |
|----|------|----------|------|
| R5a | Storage HDD/SSD 检测（rotational 文件） | ✅ 完成 | `StorageKind` 已为 `Nvme/Ssd/Hdd/Other`，sysfs.cpp 已读 `queue/rotational`，storage.cpp 已用 rotational 推断 |
| R5b | ISA 扩展枚举扩展（SSE/AES/FMA 等） | ✅ 完成 | `IsaExtension` 已有 17 个枚举值，cpu.cpp 查找表 17 条 |
| R5c | PCI device_name 填充（lspci 解析） | ✅ 完成 | `RawSource::Lspci` 已存在，pci.cpp 已用 lspci 名称覆盖 sysfs hex ID |
| R5d | Container 设计调整（移除 Virtualization.container） | ✅ 完成 | `Virtualization` 已无 `container` 字段，容器检测在 `ExecutionContext.container`（execution.cpp） |
| R5e | 版本号更新 | ✅ 完成 | 当前版本 0.0.5 |
