#!/bin/sh
# Copies the browser build (WASM_TARGET=web scripts/build-wasm.sh) into web/ next to index.html.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build-wasm-web}"
cp "$BUILD"/frontend/cemu-web.js "$BUILD"/frontend/cemu-web.wasm "$BUILD"/frontend/cemu-web.data "$ROOT/web/"
ls -la "$ROOT/web"
