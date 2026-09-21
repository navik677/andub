#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

./build.sh

if ! command -v linuxdeploy >/dev/null 2>&1; then
    echo "linuxdeploy was not found."
    echo "Install linuxdeploy, then run this script again."
    exit 1
fi

if ! command -v appimagetool >/dev/null 2>&1; then
    echo "appimagetool was not found."
    echo "Install appimagetool, then run this script again."
    exit 1
fi

APPDIR="${ROOT_DIR}/dist/Andub.AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR"

DESTDIR="$APPDIR" meson install -C build

linuxdeploy \
    --appdir "$APPDIR" \
    --desktop-file desktop/andub.desktop \
    --icon-file resources/anime-gui.png \
    --executable build/andub

mkdir -p dist
ARCH=x86_64 appimagetool "$APPDIR" "dist/Andub-x86_64.AppImage"

echo "Created dist/Andub-x86_64.AppImage"
