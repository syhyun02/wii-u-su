#!/bin/bash
# WebAssembly build of the headless frontend: build-wasm/frontend/cemu-headless.{js,wasm}
# Needs an activated emsdk and the deps from scripts/build-deps-wasm.sh.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build-wasm}"
PREFIX="${PREFIX:-$ROOT/build-wasm-deps/prefix}"
FLAGS="-pthread -fwasm-exceptions -sSUPPORT_LONGJMP=wasm -msimd128 ${EXTRA_FLAGS:-}"
if [ ! -f "$BUILD/build.ninja" ] || [ "${1:-}" = "--reconfigure" ]; then
emcmake cmake -S "$ROOT/upstream/Cemu" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PREFIX" -DCMAKE_FIND_ROOT_PATH="$PREFIX" -DBoost_NO_SYSTEM_PATHS=ON \
  -DCMAKE_C_FLAGS="$FLAGS" -DCMAKE_CXX_FLAGS="$FLAGS" \
  -DENABLE_VCPKG=OFF -DENABLE_WXWIDGETS=OFF -DENABLE_OPENGL=OFF -DENABLE_VULKAN=OFF \
  -DENABLE_CUBEB=OFF -DENABLE_SDL=OFF -DENABLE_HIDAPI=OFF -DENABLE_LIBUSB=OFF \
  -DENABLE_DISCORD_RPC=OFF -DENABLE_WAYLAND=OFF -DENABLE_BLUEZ=OFF -DENABLE_FERAL_GAMEMODE=OFF \
  -DCEMU_FRONTEND_DIR="$ROOT/frontend/headless"
fi
cmake --build "$BUILD" --target cemu-headless -- -k 0
