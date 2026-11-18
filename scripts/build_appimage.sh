#!/usr/bin/env bash
# Builds dist/Andub-x86_64.AppImage with linuxdeploy + its Qt plugin
# (bundles Qt, QML modules, platform plugins and libmpv).
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
TOOLS="$ROOT_DIR/tools"
DIST="$ROOT_DIR/dist"
APPDIR="$DIST/AppDir"

cd "$ROOT_DIR"
echo -e "\033[1;34m==> Збірка Andub...\033[0m"
cmake -S . -B build-appimage -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-appimage --parallel
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install build-appimage

mkdir -p "$TOOLS"
fetch() {
    [ -x "$TOOLS/$1" ] && return
    echo -e "\033[1;33m==> Завантаження $1...\033[0m"
    curl -fsSL -o "$TOOLS/$1" "$2"
    chmod +x "$TOOLS/$1"
}
fetch linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fetch linuxdeploy-plugin-qt-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage

export QML_SOURCES_PATHS="$ROOT_DIR/qml"
export EXTRA_QT_MODULES="svg"
export QMAKE="$(command -v qmake6 || command -v qmake)"
# Qt 6.10+ ships a single Wayland platform plugin; older versions split it in two
QT_PLATFORMS="$("$QMAKE" -query QT_INSTALL_PLUGINS)/platforms"
if [ -f "$QT_PLATFORMS/libqwayland.so" ]; then
    export EXTRA_PLATFORM_PLUGINS="libqwayland.so"
elif [ -f "$QT_PLATFORMS/libqwayland-egl.so" ]; then
    export EXTRA_PLATFORM_PLUGINS="libqwayland-egl.so;libqwayland-generic.so"
fi
export APPIMAGE_EXTRACT_AND_RUN=1
export OUTPUT="$DIST/Andub-x86_64.AppImage"

cd "$DIST"
rm -f "$OUTPUT"
"$TOOLS/linuxdeploy-x86_64.AppImage" --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/andub.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/andub.png" \
    --plugin qt --output appimage

echo -e "\033[1;32m==> AppImage:\033[0m $OUTPUT"
ls -lh "$OUTPUT"
