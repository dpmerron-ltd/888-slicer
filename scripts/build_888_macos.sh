#!/usr/bin/env bash
# Build the fork and package a relocatable macOS application.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_dir"

arch="${1:-arm64}"
case "$arch" in
    arm64|x86_64|universal) ;;
    *) echo "Usage: bash scripts/build_888_macos.sh [arm64|x86_64|universal]" >&2; exit 2 ;;
esac
jobs="${BUILD_JOBS:-4}"

# CAD is unrelated to slicing/printing. Use the same option for deps and app.
if [ "${SKIP_DEPS:-0}" != "1" ]; then
    ./build_release_macos.sh -d -x -a "$arch" -j "$jobs" -- -DSLIC3R_CAD=OFF
fi
./build_release_macos.sh -s -x -a "$arch" -j "$jobs" -- \
    -DSLIC3R_CAD=OFF -DBBL_RELEASE_TO_PUBLIC=1 -DORCA_TOOLS=OFF

bash scripts/package_888_macos.sh "$arch"
