#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
DIST_DIR="$ROOT_DIR/dist"
WIN_BUNDLE_DIR="$DIST_DIR/Anime-GUI-Windows-x86_64"

echo -e "\033[1;34m========================================================\033[0m"
echo -e "\033[1;34m    Збірка Andub для Windows (x86_64) та створення MSI  \033[0m"
echo -e "\033[1;34m========================================================\033[0m"

# 1. Prepare MinGW toolchain sysroot
CACHE_DIR="/home/ona/.cache"
MINGW_ROOT="$CACHE_DIR/mingw64_root"
if [ ! -f "$MINGW_ROOT/usr/bin/x86_64-w64-mingw32-g++" ]; then
    MINGW_ROOT="/tmp/mingw64_root"
fi

if [ ! -f "$MINGW_ROOT/usr/bin/x86_64-w64-mingw32-g++" ]; then
    echo -e "\033[1;33m==> Завантаження та підготовка MinGW-w64 пакунків...\033[0m"
    RPM_DIR="$CACHE_DIR/mingw_rpms"
    mkdir -p "$RPM_DIR"
    if [ -z "$(ls -A "$RPM_DIR" 2>/dev/null)" ]; then
        dnf download --resolve --destdir="$RPM_DIR" mingw64-gtk4 mingw64-gcc-c++ mingw64-curl mingw64-zlib
    fi
    mkdir -p "$MINGW_ROOT"
    for rpm in "$RPM_DIR"/*.rpm; do
        (cd "$MINGW_ROOT" && rpm2cpio "$rpm" | cpio -idm 2>/dev/null)
    done
fi

CXX="$MINGW_ROOT/usr/bin/x86_64-w64-mingw32-g++"
WINDRES="$MINGW_ROOT/usr/bin/x86_64-w64-mingw32-windres"
SYSROOT="$MINGW_ROOT/usr/x86_64-w64-mingw32/sys-root/mingw"

# Ensure pkgconfig files have updated prefix
sed -i "s|^prefix=.*|prefix=$SYSROOT|g" "$SYSROOT"/lib/pkgconfig/*.pc 2>/dev/null || true

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
src/utils/kodik_resolver.cpp
src/services/image_cache.cpp
src/services/player_service.cpp
src/services/upscaler.cpp
src/services/history_manager.cpp
src/services/favorites_manager.cpp
src/services/download_service.cpp
src/services/theme_manager.cpp
src/providers/anilibria_provider.cpp
src/providers/animevost_provider.cpp
src/providers/anidub_provider.cpp
src/providers/anibaza_provider.cpp
src/providers/anistar_provider.cpp
src/providers/anitube_provider.cpp
src/providers/dreamcast_provider.cpp
src/providers/shizaproject_provider.cpp
src/ui/responsive.cpp
src/ui/anime_card.cpp
src/ui/details_view.cpp
src/ui/downloads_view.cpp
src/ui/player_view.cpp
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

# Compile Windows Resource (Icon & Version Info)
echo -e "\033[1;34m==> Компіляція Windows ресурсів (andub.rc)...\033[0m"
RES_OBJ="$BUILD_DIR/andub_res.o"
$WINDRES -I"$ROOT_DIR/resources" "$ROOT_DIR/resources/andub.rc" -O coff -o "$RES_OBJ"

# 3. Link andub.exe and anime-gui.exe
echo -e "\033[1;34m==> Лінкування andub.exe (x86-64 PE GUI)...\033[0m"
WIN_EXE="$BUILD_DIR/andub.exe"
$CXX $OBJS "$RES_OBJ" -lmpv -mwindows -lws2_32 -lshlwapi -lopengl32 $PKG_LIBS -o "$WIN_EXE"
cp "$WIN_EXE" "$BUILD_DIR/anime-gui.exe"

file "$WIN_EXE"

# 4. Prepare distribution bundle
echo -e "\033[1;34m==> Формування дистрибутиву Windows...\033[0m"
rm -rf "$WIN_BUNDLE_DIR"
mkdir -p "$WIN_BUNDLE_DIR"
mkdir -p "$WIN_BUNDLE_DIR/share/glib-2.0/schemas"
mkdir -p "$WIN_BUNDLE_DIR/resources"

cp "$BUILD_DIR/andub.exe" "$WIN_BUNDLE_DIR/andub.exe"
cp "$BUILD_DIR/anime-gui.exe" "$WIN_BUNDLE_DIR/anime-gui.exe"
cp "$ROOT_DIR/resources/style.css" "$WIN_BUNDLE_DIR/resources/"
cp "$ROOT_DIR/resources/anime-gui.png" "$WIN_BUNDLE_DIR/resources/"
cp "$ROOT_DIR/resources/andub.ico" "$WIN_BUNDLE_DIR/resources/"
cp -r "$ROOT_DIR/resources/shaders" "$WIN_BUNDLE_DIR/resources/"

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
    cp -r "$SYSROOT"/share/icons "$WIN_BUNDLE_DIR/share/" 2>/dev/null || true
fi

# Create a README for Windows users
cat > "$WIN_BUNDLE_DIR/README.txt" << 'EOF'
Andub for Windows
=================
Для запуску запустіть andub.exe (або anime-gui.exe).
Всі необхідні бібліотеки (включаючи libmpv та GTK4) вже включені у дистрибутив.
EOF

# 5. Create ZIP package
echo -e "\033[1;34m==> Створення ZIP архівів...\033[0m"
cd "$DIST_DIR"
ZIP_FILE="$DIST_DIR/Andub-Windows-x86_64.zip"
rm -f "$ZIP_FILE"
zip -r -q "$ZIP_FILE" "Anime-GUI-Windows-x86_64"
cp "$ZIP_FILE" "$DIST_DIR/Anime-GUI-Windows-x86_64.zip"

# 6. Generate MSI Installer
echo -e "\033[1;34m==> Створення MSI інсталятора...\033[0m"
MSITOOLS_ROOT="$CACHE_DIR/msitools_root"
if [ ! -f "$MSITOOLS_ROOT/usr/bin/wixl" ]; then
    MSITOOLS_ROOT="/tmp/msitools_root"
fi

if [ -f "$MSITOOLS_ROOT/usr/bin/wixl" ]; then
    WIXL_BIN="$MSITOOLS_ROOT/usr/bin/wixl"
    LIB_DIR="$MSITOOLS_ROOT/usr/lib64"
else
    WIXL_BIN="wixl"
    LIB_DIR=""
fi

MSI_FILE="$DIST_DIR/Andub-Setup-x86_64.msi"
python3 "$ROOT_DIR/scripts/make_msi.py" \
    --bundle-dir "$WIN_BUNDLE_DIR" \
    --output-msi "$MSI_FILE" \
    --icon "$ROOT_DIR/resources/andub.ico" \
    --wixl "$WIXL_BIN" \
    --lib-dir "$LIB_DIR"

echo -e "\033[1;32m========================================================\033[0m"
echo -e "\033[1;32m==> Windows збірка та MSI інсталятор успішно створені:\033[0m"
echo -e "\033[1;32m========================================================\033[0m"
ls -lh "$WIN_BUNDLE_DIR/andub.exe"
ls -lh "$ZIP_FILE"
ls -lh "$MSI_FILE"
