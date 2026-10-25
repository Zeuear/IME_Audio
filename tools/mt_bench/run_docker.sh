#!/usr/bin/env bash
# @brief 在 Docker 容器里运行本目录的脚本（Bergamot 只有 Linux 预编译包；COMET 要用 GPU）。
#
# 用法：  MT_BENCH_DATA=/d/mt_bench ./run_docker.sh <cpu|gpu> <python 脚本> [参数...]
#   cpu：python:3.10-slim，装 bergamot / ctranslate2 / sacrebleu（eval_mt.py）
#   gpu：PyTorch CUDA 12.8 镜像 + unbabel-comet（score_comet.py）。RTX 50 系（sm_120）必须 CUDA 12.8 以上
#
# MT_BENCH_DATA：数据目录（flores/、models/、results/ 都在这里），默认就是本目录。
# Docker Desktop 只能挂载它能访问的盘：若本仓库所在的盘 Docker 看不到（挂进去是空目录，
# 报 "can't open file '/work/...'"），把 MT_BENCH_DATA 指到别的盘，并让 fetch_data.py 也用同一个目录。
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
DATA="${MT_BENCH_DATA:-$HERE}"
mode="$1"; script="$2"; shift 2

# 脚本复制到数据目录，容器里统一从 /work 运行
if [ "$DATA" != "$HERE" ]; then cp "$HERE"/*.py "$DATA"/; fi
[ -f "$DATA/$script" ] || { echo "数据目录里找不到 $script：$DATA（Docker 是否能访问这个盘？）" >&2; exit 1; }

win_data="$(cygpath -w "$DATA" 2>/dev/null || echo "$DATA")"
if [ "$mode" = gpu ]; then
  image=pytorch/pytorch:2.7.1-cuda12.8-cudnn9-runtime
  gpu=(--gpus all)
  setup="pip install -q unbabel-comet 2>&1 | grep -iv 'notice\|warning' | tail -2"
else
  image=python:3.10-slim
  gpu=()
  setup="pip install -q bergamot ctranslate2 sentencepiece sacrebleu 2>&1 | grep -v notice | tail -1"
fi
MSYS_NO_PATHCONV=1 docker run --rm "${gpu[@]}" -v "$win_data:/work" "$image" \
  sh -c "$setup; cd /work && python $script $*"
