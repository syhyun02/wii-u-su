#!/bin/sh
# Native (Linux x86_64) headless build: build-native/frontend/cemu-headless
# Needs: clang, cmake, ninja, boost, fmt>=12.1, pugixml, rapidjson, libzip, zstd, glslang, curl, openssl, glm, libpng, EGL/GL headers
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build-native}"
cmake -S "$ROOT/upstream/Cemu" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DENABLE_VCPKG=OFF -DENABLE_WXWIDGETS=OFF -DENABLE_OPENGL=OFF -DENABLE_VULKAN=OFF \
  -DENABLE_CUBEB=OFF -DENABLE_SDL=OFF -DENABLE_HIDAPI=OFF -DENABLE_LIBUSB=OFF \
  -DENABLE_DISCORD_RPC=OFF -DENABLE_WAYLAND=OFF -DENABLE_BLUEZ=OFF -DENABLE_FERAL_GAMEMODE=OFF \
  -DCEMU_FRONTEND_DIR="$ROOT/frontend/headless"
cmake --build "$BUILD" --target cemu-headless
