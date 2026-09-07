#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
DIST_DIR="$ROOT_DIR/dist"
WIN_BUNDLE_DIR="$DIST_DIR/Anime-GUI-Windows-x86_64"

echo -e "\033[1;34m==> Початок збірки Windows версії Anime GUI (x86_64)...\033[0m"

# 1. Prepare MinGW toolchain sysroot
MINGW_ROOT="/tmp/mingw64_root"
if [ ! -f "$MINGW_ROOT/usr/bin/x86_64-w64-mingw32-g++" ]; then
    echo -e "\033[1;33m==> Завантаження та підготовка MinGW-w64 пакунків...\033[0m"
    RPM_DIR="/tmp/test_mingw"
    if [ ! -d "$RPM_DIR" ] || [ -z "$(ls -A "$RPM_DIR" 2>/dev/null)" ]; then
        mkdir -p "$RPM_DIR"
        dnf download --resolve --destdir="$RPM_DIR" mingw64-gtk4 mingw64-gcc-c++ mingw64-curl mingw64-zlib
    fi
    mkdir -p "$MINGW_ROOT"
    for rpm in "$RPM_DIR"/*.rpm; do
        (cd "$MINGW_ROOT" && rpm2cpio "$rpm" | cpio -idm 2>/dev/null)
    done
fi

CXX="$MINGW_ROOT/usr/bin/x86_64-w64-mingw32-g++"
SYSROOT="$MINGW_ROOT/usr/x86_64-w64-mingw32/sys-root/mingw"

# Ensure pkgconfig files have updated prefix
sed -i "s|^prefix=.*|prefix=$SYSROOT|g" "$SYSROOT"/lib/pkgconfig/*.pc 2>/dev/null || true
while grep -q "/tmp/mingw64_root/tmp/mingw64_root" "$SYSROOT"/lib/pkgconfig/*.pc 2>/dev/null; do
    sed -i "s|/tmp/mingw64_root/tmp/mingw64_root|/tmp/mingw64_root|g" "$SYSROOT"/lib/pkgconfig/*.pc
done

PKG_FLAGS=$(PKG_CONFIG_LIBDIR="$SYSROOT/lib/pkgconfig" pkg-config --cflags gtk4 libcurl)
PKG_LIBS=$(PKG_CONFIG_LIBDIR="$SYSROOT/lib/pkgconfig" pkg-config --libs gtk4 libcurl)

# 2. Compile sources for Windows
BUILD_DIR="$ROOT_DIR/build_win"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

SOURCES="
src/main.cpp
src/application.cpp
src/utils/http_client.cpp
src/services/image_cache.cpp
src/services/player_service.cpp
src/services/history_manager.cpp
src/services/favorites_manager.cpp
src/services/download_service.cpp
src/services/theme_manager.cpp
src/providers/anilibria_provider.cpp
src/providers/animevost_provider.cpp
src/ui/anime_card.cpp
src/ui/details_view.cpp
src/ui/downloads_view.cpp
src/ui/main_window.cpp
"

echo -e "\033[1;34m==> Компіляція C++ джерел для Windows (x86-64)...\033[0m"
OBJS=""
for src in $SOURCES; do
    obj_name="$(echo "$src" | tr '/' '_').o"
    obj="$BUILD_DIR/$obj_name"
    echo "  -> $src"
    $CXX -std=c++20 -O2 -Isrc -DHAVE_LIBCURL $PKG_FLAGS -c "$ROOT_DIR/$src" -o "$obj"
    OBJS="$OBJS $obj"
done

# 3. Link anime-gui.exe
echo -e "\033[1;34m==> Лінкування anime-gui.exe...\033[0m"
WIN_EXE="$BUILD_DIR/anime-gui.exe"
$CXX $OBJS $PKG_LIBS -mwindows -lws2_32 -lshlwapi -o "$WIN_EXE"

file "$WIN_EXE"

# 4. Prepare distribution bundle
echo -e "\033[1;34m==> Формування дистрибутиву Windows...\033[0m"
rm -rf "$WIN_BUNDLE_DIR"
mkdir -p "$WIN_BUNDLE_DIR"
mkdir -p "$WIN_BUNDLE_DIR/share/glib-2.0/schemas"
mkdir -p "$WIN_BUNDLE_DIR/resources"

cp "$WIN_EXE" "$WIN_BUNDLE_DIR/anime-gui.exe"
cp "$ROOT_DIR/resources/style.css" "$WIN_BUNDLE_DIR/resources/"
cp "$ROOT_DIR/resources/anime-gui.png" "$WIN_BUNDLE_DIR/resources/"

# Copy all required DLLs from sysroot bin
echo -e "\033[1;34m==> Копіювання необхідних DLL бібліотек...\033[0m"
cp "$SYSROOT"/bin/*.dll "$WIN_BUNDLE_DIR/" 2>/dev/null || true

# Copy schemas & compile
if [ -d "$SYSROOT/share/glib-2.0/schemas" ]; then
    cp -r "$SYSROOT"/share/glib-2.0/schemas/* "$WIN_BUNDLE_DIR/share/glib-2.0/schemas/" 2>/dev/null || true
    glib-compile-schemas "$WIN_BUNDLE_DIR/share/glib-2.0/schemas" 2>/dev/null || true
fi

# Copy icons
if [ -d "$SYSROOT/share/icons" ]; then
    cp -r "$SYSROOT/share/icons" "$WIN_BUNDLE_DIR/share/" 2>/dev/null || true
fi

# Create a README for Windows users
cat > "$WIN_BUNDLE_DIR/README.txt" << 'EOF'
Anime GUI for Windows
=====================
Для запуску запустіть anime-gui.exe.
Для відтворення відео рекомендується мати встановлений mpv (або додати mpv.exe у PATH або в цю ж папку).
EOF

# 5. Create ZIP package
echo -e "\033[1;34m==> Створення ZIP архіву...\033[0m"
cd "$DIST_DIR"
ZIP_FILE="$DIST_DIR/Anime-GUI-Windows-x86_64.zip"
rm -f "$ZIP_FILE"
zip -r -q "$ZIP_FILE" "Anime-GUI-Windows-x86_64"

echo -e "\033[1;32m==> Windows збірка успішно завершена:\033[0m"
ls -lh "$WIN_BUNDLE_DIR/anime-gui.exe"
ls -lh "$ZIP_FILE"
