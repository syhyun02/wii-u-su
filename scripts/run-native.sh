#!/bin/sh
# Usage: scripts/run-native.sh <game dir / .wua / .wud / .rpx> [extra cemu args]
# Saves, settings and mlc01 go to $CEMU_HOME (default: run/home, git-ignored).
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAME="$1"; shift
export CEMU_HOME="${CEMU_HOME:-$ROOT/run/home}"
export CEMU_DATA="${CEMU_DATA:-$ROOT/upstream/Cemu/bin}"
exec "${BUILD:-$ROOT/build-native}/frontend/cemu-headless" -g "$GAME" --forward-console-logging "$@"
