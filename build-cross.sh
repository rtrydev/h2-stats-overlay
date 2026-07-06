#!/usr/bin/env bash
#
# Cross-compile the ASI plugin from macOS/Linux using MinGW-w64 (32-bit).
# The Windows build uses build.ps1 (MSVC); this produces an equivalent binary.
#
# IMPORTANT: the -msse2 -mfpmath=sse flags are not optional. MSVC compiles
# 32-bit float math to SSE2 by default, but MinGW defaults to the x87 FPU.
# x87 is emulated slowly by translation layers (e.g. CrossOver/Rosetta on Mac),
# which tanks the in-game framerate every frame the overlay does float math.
# Building with SSE matches MSVC and keeps the overlay cheap. -mstackrealign
# makes 16-byte-aligned SSE moves safe when the game calls us on a 4-byte stack.
#
# Requires: i686-w64-mingw32-g++ (e.g. `brew install mingw-w64`).

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="$ROOT/build/Release"
OUT="$OUT_DIR/h2_stats_overlay.asi"

CXX="${MINGW_CXX:-i686-w64-mingw32-g++}"
if ! command -v "$CXX" >/dev/null 2>&1; then
    echo "error: $CXX not found (install mingw-w64, e.g. 'brew install mingw-w64')" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

"$CXX" \
    -std=c++17 -O2 \
    -msse2 -mfpmath=sse -mstackrealign \
    -s -shared -static -static-libgcc -static-libstdc++ \
    -DWIN32 -D_WINDOWS -DNDEBUG \
    -I"$ROOT/src" \
    "$ROOT"/src/*.cpp \
    -o "$OUT" \
    -Wl,--subsystem,windows -Wl,--enable-stdcall-fixup \
    -lkernel32 -luser32

echo "Built:"
echo "  $OUT"
