#!/usr/bin/env bash
# 在已配置好的 bergamot-translator 构建目录里编译 translate_worker（Windows，OpenBLAS 加锁版）。
# 前置：按 docs/local-translation-spec.md 附录 A 完成 OpenBLAS 覆盖端口与 build_ob_lock 的配置。
# 用法： tools/translate_worker/build_win.sh [bergamot 根目录，默认 /d/bergamot] [构建目录名，默认 build_ob_lock]
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${1:-/d/bergamot}"
BUILD="${2:-build_ob_lock}"
APP_CMAKE="$ROOT/src/app/CMakeLists.txt"
WORKER_DIR="$(cygpath -m "$HERE")"

# 把本目录挂到 bergamot 的 app 子目录下（只加一次）
if ! grep -q "translate_worker" "$APP_CMAKE"; then
  printf '\nadd_subdirectory(%s translate_worker)\n' "$WORKER_DIR" >> "$APP_CMAKE"
fi

cd "$ROOT"
cmake -S src -B "$BUILD" > "logs/translate_worker_configure.log" 2>&1 || { echo "配置失败，见 $ROOT/logs/translate_worker_configure.log"; exit 1; }
cmake --build "$BUILD" --config Release --target translate_worker > "logs/translate_worker_build.log" 2>&1 \
  || { echo "编译失败："; grep -E "error" "logs/translate_worker_build.log" | head -10; exit 1; }

ls -la "$BUILD/app/translate_worker/Release/translate_worker.exe" | awk '{printf "translate_worker.exe %.1f MB\n", $5/1e6}'
