#!/bin/sh
# Builds the test homebrew with the official devkitPro image (devkitPPC + wut).
# Output: tests/homebrew/out/<name>.rpx (git-ignored).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/out"
for dir in "$HERE"/*/; do
  name="$(basename "$dir")"
  [ "$name" = out ] && continue
  docker run --rm -v "$HERE:/src" -w /src devkitpro/devkitppc:latest sh -c \
    "/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake -S $name -B /tmp/build-$name >/dev/null && cmake --build /tmp/build-$name && cp /tmp/build-$name/$name.rpx out/"
done
ls -la "$HERE/out"
