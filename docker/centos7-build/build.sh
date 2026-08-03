#!/bin/bash
# 在 CentOS 7 容器中编译 sysal，产出 glibc 2.17 兼容的 .so / .a。
# 前提条件：已安装 Docker 并运行。
# 代理用户：设置 http_proxy / https_proxy 环境变量后运行，脚本会自动传递给容器。
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
IMAGE_NAME="sysal-centos7-build"

command -v docker >/dev/null 2>&1 || { echo "ERROR: docker not found. Install Docker first."; exit 1; }
docker info >/dev/null 2>&1 || { echo "ERROR: Docker daemon not running."; exit 1; }

echo "=== Building Docker image ==="
docker build \
    --build-arg HTTP_PROXY="${http_proxy:-}" \
    --build-arg HTTPS_PROXY="${https_proxy:-}" \
    -t "$IMAGE_NAME" \
    "$SCRIPT_DIR"

echo "=== Running build in container ==="
DOCKER_NET="--network host"
if [ "${CI:-}" = "true" ]; then
    DOCKER_NET=""   # GitHub Actions 禁用 host 网络；容器直接走 bridge（默认）公网
fi
docker run --rm \
    $DOCKER_NET \
    -e http_proxy="${http_proxy:-}" \
    -e https_proxy="${https_proxy:-}" \
    -v "$PROJECT_ROOT:/workspace" \
    -w /workspace \
    "$IMAGE_NAME" \
    bash /workspace/docker/centos7-build/build_inside.sh

echo "=== Done ==="
echo "Artifacts:"
echo "  $PROJECT_ROOT/build/linux/x86_64/release/libsysal.so"
echo "  $PROJECT_ROOT/build/linux/x86_64/release/static/libsysal.a"
