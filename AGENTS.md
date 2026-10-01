# sysal

C++ 系统信息抽象库，采集、归一化并暴露服务器系统信息。

## 环境与构建

- C++20 / xmake；默认 Clang，GCC 可显式选择。
- 使用所选工具链的默认标准库、链接器和 runtime。
- glibc 2.17+ 发布产物由 CentOS 7 / GCC 兼容构建提供。
- 版本唯一来源为 `include/sysal/version.hpp`，xmake 从该文件读取版本。

```bash
xmake f -c -y --toolchain=clang
xmake build
xmake run sysal_info
```

普通构建包含静态库、动态库和示例；测试由 `xmake test` 构建并执行。
`compile_commands.json` 由 xmake 自动更新到 `build/`。

## 格式与完整验证

```bash
xmake format       # 显式修改格式
xmake check        # 只验证：format check + tidy + rebuild + test
xmake test         # doctest 单元测试与独立 replay 集成测试
```

- format / tidy 覆盖 `include/`、`src/`、`tests/`、`examples/` 的 C/C++ 文件。
- 编译使用 `-Wall -Wextra -Werror -Wpedantic`；clang-tidy warnings 全部视为 error。
- 必要的 lint 例外须局部标注并说明原因，不扩大全局禁用范围。
- clang-format 行宽 120，缩进 4 空格；文本统一 LF。
- `<cctype>` 函数传参须 `static_cast<unsigned char>()`。
- 命名规则参见 `docs/design/rules/strong_typing.md`。
- 检查不得修改 tracked 文件或 index；格式修复单独执行。
- 开发记录写在 Git 提交中，不要求维护手工开发日志。

## Git hook 与 CI

```bash
git config core.hooksPath .githooks
```

pre-commit 只验证 staged snapshot 的空白与 C/C++ 格式，不修改工作区或 index。
构建不自动配置 hook。CI 使用 Clang 运行 `xmake check`，断言工作区与 index 未变，
随后运行 `xmake run sysal_info` 冒烟测试。

## 修改后的验证

代码或配置变更后执行 `xmake check` 和 `xmake run sysal_info`。
工具链或构建行为变更须从 clean configuration 验证相关工具链。
涉及兼容产物时执行 `bash docker/centos7-build/build.sh`，检查 glibc 2.17 兼容性。
