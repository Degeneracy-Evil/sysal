# sysal

C++ 系统信息抽象库 — 收集、归一化、暴露服务器系统信息。

## 构建

```bash
xmake build          # 构建（静态库 + 动态库 + 全部测试）
xmake -r             # 重新构建
xmake sysal_info     # 编译并运行 sysal_info，终端输出
```

`compile_commands.json` 由 xmake 的 `plugin.compile_commands.autoupdate` 规则根据真实构建参数自动更新到 `build/`（供 clang-tidy / clangd 使用），无需手动生成。

## 项目结构

```
include/sysal/    公共头文件
src/              源文件
tests/            测试样例
docs/             文档
docker/
  centos7-build/  CentOS 7 兼容性构建（Dockerfile + build.sh）
.githooks/
  pre-commit      自动修复格式 + 尾随空白并重新暂存
.github/
  workflows/
    ci.yml        CI 流水线（push/PR 触发 xmake check）
output/           输出文件 (gitignore)
```

## 关键约定

- C++20 / xmake / 自适应工具链（clang+libc++ 或 gcc+libstdc++）
- GCC 使用系统默认标准库和链接器；Clang 使用 libc++ / lld / compiler-rt / libunwind
- 编译选项 -Wall -Wextra -Werror，零 warning
- clang-tidy `WarningsAsErrors: '*'`，静态分析零容忍
- `<cctype>` 函数传参必须 `static_cast<unsigned char>()`，否则 signed char 有 UB
- `compile_commands.json` 由 xmake 的 `plugin.compile_commands.autoupdate` 规则自动更新
- clang-format / clang-tidy 递归检查 `include/`、`src/`、`tests/` 下的 C/C++ 文件
- 行尾统一 LF（`.gitattributes` 控制）
- `xmake check` 运行全量质量检查（format + tidy + rebuild + test）
- `xmake test` 运行全部单元和集成测试
- 版本号定义在 `include/sysal/version.hpp`（VERSION_MAJOR / VERSION_MINOR / VERSION_PATCH / VERSION_STRING）
- pre-commit hook 自动修复格式 + 尾随空白并重新暂存，不阻塞提交；首次 `xmake build` 自动配置 `core.hooksPath`
- 项目未配置 pre-push hook；push 后由 GitHub Actions 执行 `xmake check` 并运行 `xmake run sysal_info` 冒烟测试
- CI 只使用 Clang，不设置编译器矩阵；xmake 使用 latest，避免在 CI 中重复单独构建
- 命名规则参见 `docs/design/rules/strong_typing.md`

## 兼容性构建

预编译产物兼容 glibc 2.17+（CentOS 7 / RHEL 7 及所有主流 Linux 发行版）。

```bash
bash docker/centos7-build/build.sh   # 在 CentOS 7 容器中编译，产出 glibc 2.17 兼容的 .so/.a
```

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
