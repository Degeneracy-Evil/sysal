# 版本固定与 GitHub Release 计划

## 状态：已完成

版本号集中管理已在 v0.0.2 实现，后续版本正常递增发布。

| 版本 | 发布状态 | 主要内容 |
|------|----------|----------|
| v0.0.1 | ✅ 已发布 | 首版：基础采集 + 序列化 |
| v0.0.2 | ✅ 已发布 | R1 bug 修复 + R2 重构 + R4 testbench + R6 文档 |
| v0.0.3 | ✅ 已发布 | nlohmann/json 替换手写 JSON |
| v0.0.4 | ✅ 已发布 | 代码质量评审 + P1/P2 修复 |
| v0.0.5 | ✅ 已发布 | CentOS 7 兼容性构建 + 虚拟化检测 + Doxygen + 脚手架迁移 |

当前版本号定义在 `include/sysal/version.hpp`（VERSION_MAJOR / VERSION_MINOR / VERSION_PATCH / VERSION_STRING），xmake.lua 不再维护 `set_version()`。
