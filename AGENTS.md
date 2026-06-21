# sysal

C++ 系统信息抽象库 — 收集、归一化、暴露服务器系统信息。

## 构建

```bash
xmake build          # 构建
xmake -r             # 重新构建
```

`compile_commands.json` 在构建后自动生成到 `build/`（供 clang-tidy / clangd 使用），无需手动运行。

## 项目结构

```
include/sysal/    公共头文件
src/              源文件
tests/            测试样例
docs/             文档
utils/
  check.sh        一键检查（clang-format + clang-tidy + build + tests）
.githooks/
  pre-commit      转发到 check.sh --hook
.github/
  workflows/
    ci.yml        CI 流水线（push/PR 触发 check.sh）
output/           输出文件 (gitignore)
```

## 关键约定

- C++23 / xmake / clang++ / libc++ / lld / compiler-rt
- 编译选项 -Wall -Wextra -Werror，零 warning
- clang-tidy `WarningsAsErrors: '*'`，静态分析零容忍
- `<cctype>` 函数传参必须 `static_cast<unsigned char>()`，否则 signed char 有 UB
- `compile_commands.json` 由 `after_build` 钩子自动生成，`xmake -r` 后也会重新生成
- 行尾统一 LF（`.gitattributes` 控制）
- pre-commit hook 通过 `.githooks/pre-commit` 转发到 `utils/check.sh --hook`；首次 `xmake build` 自动配置 `core.hooksPath`
- 命名规则参见 `docs/命名规则.md`

## 开发记录规则

**每次文档或代码变动，必须在 `docs/devlog.md` 中留存痕迹。**

记录格式：
```
### YYYY-MM-DD 简述

- **变更类型**: docs / src / fix / refactor / build / chore
- **涉及文件**: 文件列表
- **变更内容**: 具体做了什么
- **原因**: 为什么做这个变更
- **验证**: 如何验证正确性（测试命令/结果）
```
