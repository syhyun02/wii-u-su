#!/bin/sh
# Puts upstream/Cemu on local branch "wasm" = pinned upstream commit + patches/cemu/*.patch
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$(git -C "$ROOT" ls-tree HEAD upstream/Cemu | awk '{print $3}')"
cd "$ROOT/upstream/Cemu"
git checkout -q -B wasm "$BASE"
git -c user.name=patches -c user.email=patches@localhost am -q "$ROOT"/patches/cemu/*.patch
git submodule update --init --depth 1 dependencies/ZArchive dependencies/imgui dependencies/Vulkan-Headers
git log --oneline "$BASE"..HEAD
