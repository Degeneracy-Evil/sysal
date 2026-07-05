#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
IMAGE_NAME="sysal-centos7-build"
CONTAINER_NAME="sysal-centos7-build-run"

echo "=== Building Docker image ==="
docker build -t "$IMAGE_NAME" "$SCRIPT_DIR"

echo "=== Running build in container ==="
docker run --rm \
    --network host \
    -e http_proxy="${http_proxy:-}" \
    -e https_proxy="${https_proxy:-}" \
    -v "$PROJECT_ROOT:/workspace" \
    -w /workspace \
    --name "$CONTAINER_NAME" \
    "$IMAGE_NAME" \
    bash -c '
        set -euo pipefail
        echo "--- Toolchain versions ---"
        gcc --version | head -1
        g++ --version | head -1
        xmake --version | head -1

        echo "--- Building sysal ---"
        export XMAKE_ROOT=y
        xmake f -c -y --toolchain=gcc
        xmake -r

        echo "--- Verifying glibc symbol versions ---"
        echo "=== libsysal.so ==="
        objdump -T build/linux/x86_64/release/libsysal.so | grep -o "GLIBC_[0-9.]*" | sort -V -u
        echo "=== libsysal.a ==="
        objdump -T build/linux/x86_64/release/static/libsysal.a | grep -o "GLIBC_[0-9.]*" | sort -V -u

        echo "--- Checking for GLIBC > 2.17 ---"
        BAD=$(objdump -T build/linux/x86_64/release/libsysal.so | grep -o "GLIBC_[0-9.]*" | sort -V -u | while read v; do
            major=$(echo "$v" | cut -d_ -f2 | cut -d. -f1)
            minor=$(echo "$v" | cut -d. -f2)
            patch=$(echo "$v" | cut -d. -f3)
            if [ "$major" -gt 2 ] || ([ "$major" -eq 2 ] && [ "$minor" -gt 17 ]); then
                echo "$v"
            fi
        done)
        if [ -z "$BAD" ]; then
            echo "PASS: All GLIBC symbols <= 2.17"
        else
            echo "FAIL: Found GLIBC symbols > 2.17:"
            echo "$BAD"
            exit 1
        fi

        echo "--- Checking libstdc++ symbol versions ==="
        objdump -T build/linux/x86_64/release/libsysal.so | grep -o "GLIBCXX_[0-9.]*" | sort -V -u || echo "(none)"
        objdump -T build/linux/x86_64/release/libsysal.so | grep -o "CXXABI_[0-9.]*" | sort -V -u || echo "(none)"

        echo "--- ldd ==="
        ldd build/linux/x86_64/release/libsysal.so
    '

echo "=== Build complete ==="
echo "Artifacts:"
echo "  build/linux/x86_64/release/libsysal.so"
echo "  build/linux/x86_64/release/static/libsysal.a"
