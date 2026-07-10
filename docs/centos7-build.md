# CentOS 7 兼容性构建

sysal 预编译产物兼容 glibc 2.17+，覆盖 CentOS 7 / RHEL 7 及所有主流 Linux 发行版（Ubuntu 18.04+、Debian 10+、SUSE SLES 12+、Amazon Linux 2+）。

## 快速使用

```bash
bash docker/centos7-build/build.sh
```

产物位于 `build/linux/x86_64/release/` 目录下。

## 脚本流程

1. `build.sh` 构建 Docker 镜像：centos:7 + devtoolset-11 (GCC 11.2.1) + xmake
2. 在容器中挂载项目源码，调用 `build_inside.sh` 执行编译
3. 运行 `sysal_info` 验证产物在 CentOS 7 环境下可正常执行

## 文件说明

| 文件 | 作用 |
|------|------|
| `build.sh` | 宿主机入口，构建镜像并启动容器 |
| `build_inside.sh` | 容器内执行，编译 + 运行验证 |
| `Dockerfile` | 镜像定义 |

## 为什么从源码编译 xmake

CentOS 7 的 ncurses 版本过低，xmake 预编译二进制依赖 ncurses.so.6 无法运行。因此从源码编译 xmake（`ARG XMAKE_VERSION` 可调）。

## 为什么切换到 vault 源

CentOS 7 已 EOL，官方镜像源停止维护。Dockerfile 中将 mirrorlist 替换为 vault.centos.org。

## 代理支持

脚本通过 `--build-arg` 和 `-e` 将宿主机的 `http_proxy` / `https_proxy` 传递给 Docker 构建和运行环境。使用 `--network host` 确保容器能访问宿主机本地代理。

```bash
export http_proxy=http://127.0.0.1:7891
export https_proxy=http://127.0.0.1:7891
bash docker/centos7-build/build.sh
```

## 已知限制

容器挂载项目根目录，`xmake f --toolchain=gcc` 会改写 `.xmake/` 配置。回到宿主机后需重新 `xmake f -c` 切回原工具链。
