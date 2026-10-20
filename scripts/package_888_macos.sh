#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_dir"
arch="${1:-arm64}"
case "$arch" in
    arm64|x86_64|universal) ;;
    *) echo "Usage: bash scripts/package_888_macos.sh [arm64|x86_64|universal]" >&2; exit 2 ;;
esac
source_app="$repo_dir/build/$arch/OrcaSlicer/OrcaSlicer.app"
if [ ! -x "$source_app/Contents/MacOS/OrcaSlicer" ]; then
    echo "Missing built application: $source_app" >&2
    exit 1
fi
if [ "$arch" = "universal" ]; then
    lipo -verify_arch arm64 x86_64 "$source_app/Contents/MacOS/OrcaSlicer"
else
    lipo -verify_arch "$arch" "$source_app/Contents/MacOS/OrcaSlicer"
fi

mkdir -p "$repo_dir/dist"
stage_dir="$(mktemp -d "$repo_dir/dist/888-slicer-$arch.XXXXXX")"
app="$stage_dir/888 Slicer.app"
ditto "$source_app" "$app"
cp "$repo_dir/config/printers.example.json" "$stage_dir/printers.example.json"
cp "$repo_dir/docs/888-slicer.md" "$stage_dir/README.md"
cp "$repo_dir/LICENSE.txt" "$stage_dir/LICENSE.txt"
ln -s /Applications "$stage_dir/Applications"

# An ad-hoc signature makes the bundle consistent on the build machine. A
# Developer ID signature + notarization is required for trusted distribution.
identity="${MACOS_SIGN_IDENTITY:--}"
sign_options=(--force --deep --sign "$identity")
if [ "$identity" != "-" ]; then
    sign_options+=(--timestamp --options runtime)
fi
codesign "${sign_options[@]}" \
    --entitlements "$repo_dir/src/dev-utils/platform/osx/entitlements.plist" "$app"
codesign --verify --deep --strict "$app"

stamp="$(git rev-parse --short HEAD)"
dmg="$repo_dir/dist/888-Slicer-$arch-$stamp.dmg"
if [ -e "$dmg" ]; then
    dmg="$repo_dir/dist/888-Slicer-$arch-$stamp-$(date +%Y%m%d-%H%M%S).dmg"
fi
hdiutil create -volname "888 Slicer" -srcfolder "$stage_dir" -format UDZO "$dmg"

# This profile is configured by the distributor in Keychain, never in this repo.
if [ -n "${MACOS_NOTARY_PROFILE:-}" ]; then
    if [ "$identity" = "-" ]; then
        echo "Notarization requires MACOS_SIGN_IDENTITY (Developer ID Application)." >&2
        exit 1
    fi
    xcrun notarytool submit "$dmg" --keychain-profile "$MACOS_NOTARY_PROFILE" --wait
    xcrun stapler staple "$dmg"
fi
shasum -a 256 "$dmg"
printf 'Packaged application: %s\nDisk image: %s\n' "$app" "$dmg"
