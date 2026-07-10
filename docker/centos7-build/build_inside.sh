#!/bin/bash
# 在 CentOS 7 容器内执行的构建脚本。
set -euo pipefail

echo "--- Toolchain versions ---"
gcc --version | head -1
g++ --version | head -1
xmake --version | head -1

echo "--- Building sysal ---"
xmake f -c -y --toolchain=gcc
xmake -r

LIBSO=build/linux/x86_64/release/libsysal.so
LIBA=build/linux/x86_64/release/static/libsysal.a

for f in "$LIBSO" "$LIBA"; do
    [ -f "$f" ] || { echo "ERROR: $f not found"; exit 1; }
done

echo "--- Build artifacts ---"
echo "  $LIBSO"
echo "  $LIBA"

echo "--- Running sysal_info (compatibility verification) ---"
xmake run sysal_info

echo "--- Build complete ---"
