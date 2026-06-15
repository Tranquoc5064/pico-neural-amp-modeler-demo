#!/usr/bin/env bash
# Build the A2-fast host verifier against NeuralAmpModelerCore @ main, with the
# a2_fast path and inline GEMM enabled (matching the device build flags).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CORE="$ROOT/lib/NeuralAmpModelerCore"
OUT="${1:-/tmp/host_a2}"

c++ -std=c++17 -O2 -ffast-math -DNAM_SAMPLE_FLOAT=1 -DNAM_ENABLE_A2_FAST=1 -DNAM_USE_INLINE_GEMM=1 \
    -I"$CORE" -I"$CORE/Dependencies/eigen" -I"$CORE/Dependencies/nlohmann" \
    "$CORE"/NAM/*.cpp "$CORE"/NAM/wavenet/*.cpp \
    "$ROOT"/tools/host_a2.cpp \
    -o "$OUT"
echo "built $OUT"
