#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

echo -e "\033[1;34m==> Початок збірки Linux AppImage для Anime GUI...\033[0m"

# 1. Build application binary if needed
cd "$ROOT_DIR"
if [ ! -f "build/anime-gui" ]; then
    echo -e "\033[1;33m==> Збірка бінарника anime-gui через Ninja...\033[0m"
    ninja -C build
fi

# 2. Check or obtain appimagetool
APPIMAGETOOL=""
if command -v appimagetool &>/dev/null; then
    APPIMAGETOOL="appimagetool"
elif [ -f "$ROOT_DIR/tools/squashfs-root/AppRun" ]; then
    APPIMAGETOOL="$ROOT_DIR/tools/squashfs-root/AppRun"
elif [ -f "$ROOT_DIR/squashfs-root/AppRun" ]; then
    APPIMAGETOOL="$ROOT_DIR/squashfs-root/AppRun"
else
    echo -e "\033[1;33m==> Завантаження appimagetool...\033[0m"
    mkdir -p "$ROOT_DIR/tools"
    curl -sL -o "$ROOT_DIR/tools/appimagetool" "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
    chmod +x "$ROOT_DIR/tools/appimagetool"
    (cd "$ROOT_DIR/tools" && ./appimagetool --appimage-extract >/dev/null 2>&1)
    APPIMAGETOOL="$ROOT_DIR/tools/squashfs-root/AppRun"
fi

# 3. Prepare AppDir structure
DIST_DIR="$ROOT_DIR/dist"
APPDIR="$DIST_DIR/AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$APPDIR/usr/share/glib-2.0/schemas"

# 4. Copy binary and assets
cp "$ROOT_DIR/build/anime-gui" "$APPDIR/usr/bin/"
chmod +x "$APPDIR/usr/bin/anime-gui"

cp "$ROOT_DIR/resources/anime-gui.png" "$APPDIR/anime-gui.png"
cp "$ROOT_DIR/resources/anime-gui.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/anime-gui.png"

cp "$ROOT_DIR/desktop/anime-gui.desktop" "$APPDIR/anime-gui.desktop"
cp "$ROOT_DIR/desktop/anime-gui.desktop" "$APPDIR/usr/share/applications/anime-gui.desktop"

# 5. Copy GTK4 GSettings schemas
if [ -d "/usr/share/glib-2.0/schemas" ]; then
    cp -r /usr/share/glib-2.0/schemas/*gtk* "$APPDIR/usr/share/glib-2.0/schemas/" 2>/dev/null || true
    glib-compile-schemas "$APPDIR/usr/share/glib-2.0/schemas" 2>/dev/null || true
fi

# 6. Copy bundled shared libraries (excluding core system glibc / driver libs)
echo -e "\033[1;34m==> Копіювання залежних динамічних бібліотек...\033[0m"
EXCLUDE_REGEX="libc\.so|libm\.so|libpthread\.so|libdl\.so|librt\.so|ld-linux|libGL|libEGL|libvulkan|libdrm|libX11|libxcb"

for lib in $(ldd "$ROOT_DIR/build/anime-gui" | awk '{if ($3 ~ /^\//) print $3}'); do
    lib_name=$(basename "$lib")
    if ! echo "$lib_name" | grep -qE "$EXCLUDE_REGEX"; then
        if [ -f "$lib" ] && [ ! -f "$APPDIR/usr/lib/$lib_name" ]; then
            cp -L "$lib" "$APPDIR/usr/lib/" 2>/dev/null || true
        fi
    fi
done

# 7. Create AppRun launcher
cat > "$APPDIR/AppRun" << 'EOF'
#!/usr/bin/env bash
set -e

HERE="$(dirname "$(readlink -f "${0}")")"
export APPDIR="${HERE}"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"
export XDG_DATA_DIRS="${HERE}/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
export GSETTINGS_SCHEMA_DIR="${HERE}/usr/share/glib-2.0/schemas:${GSETTINGS_SCHEMA_DIR}"

exec "${HERE}/usr/bin/anime-gui" "$@"
EOF
chmod +x "$APPDIR/AppRun"

# 8. Build AppImage
echo -e "\033[1;34m==> Пакування AppImage за допомогою appimagetool...\033[0m"
mkdir -p "$DIST_DIR"
OUTPUT_APPIMAGE="$DIST_DIR/Anime-GUI-x86_64.AppImage"
rm -f "$OUTPUT_APPIMAGE"

RUNTIME_ARG=""
if [ -f "$ROOT_DIR/tools/runtime-x86_64" ]; then
    RUNTIME_ARG="--runtime-file $ROOT_DIR/tools/runtime-x86_64"
fi

ARCH=x86_64 "$APPIMAGETOOL" $RUNTIME_ARG "$APPDIR" "$OUTPUT_APPIMAGE"

chmod +x "$OUTPUT_APPIMAGE"

echo -e "\033[1;32m==> AppImage успішно створено:\033[0m $OUTPUT_APPIMAGE"
ls -lh "$OUTPUT_APPIMAGE"
