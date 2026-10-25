#!/usr/bin/env bash
# ============================================================
# 构建脚本（Linux / WSL）
#
#   ./scripts/build.sh              # RelWithDebInfo 构建
#   ./scripts/build.sh Debug        # Debug 构建
#   ./scripts/build.sh Release clean # 清理后重建
# ============================================================
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="${1:-RelWithDebInfo}"
DO_CLEAN="${2:-}"

cd "${ROOT_DIR}"

if [[ "${DO_CLEAN}" == "clean" ]]; then
    echo ">> 清理 build 目录"
    rm -rf build
fi

echo ">> 配置 (${BUILD_TYPE})"
cmake -S . -B build -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

echo ">> 编译"
cmake --build build -j"$(nproc)"

echo
echo ">> 完成: build/bin/rtsp_server"
ls -lh build/bin/rtsp_server 2>/dev/null || true
