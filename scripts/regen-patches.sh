#!/bin/sh
# Regenerates patches/cemu/*.patch from the commits on upstream/Cemu branch "wasm".
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$(git -C "$ROOT" ls-tree HEAD upstream/Cemu | awk '{print $3}')"
rm -f "$ROOT"/patches/cemu/*.patch
git -C "$ROOT/upstream/Cemu" format-patch -q -o "$ROOT/patches/cemu" "$BASE"..wasm
ls "$ROOT/patches/cemu"
