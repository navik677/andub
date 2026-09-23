#!/usr/bin/env bash

set -e

echo -e "\033[1;36m==> Встановлення Anime GUI (C++ GTK4)...\033[0m"

# 1. Перевірка компілятора та системних інструментів
MISSING_DEPS=""
if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then MISSING_DEPS="g++ $MISSING_DEPS"; fi
if ! command -v meson &> /dev/null; then MISSING_DEPS="meson $MISSING_DEPS"; fi
if ! command -v ninja &> /dev/null; then MISSING_DEPS="ninja $MISSING_DEPS"; fi
if ! command -v pkg-config &> /dev/null; then MISSING_DEPS="pkg-config $MISSING_DEPS"; fi
if ! pkg-config --exists gtk4; then MISSING_DEPS="gtk4-devel $MISSING_DEPS"; fi
if ! pkg-config --exists libcurl; then MISSING_DEPS="libcurl-devel $MISSING_DEPS"; fi
if ! pkg-config --exists mpv; then MISSING_DEPS="libmpv-dev $MISSING_DEPS"; fi

if [ -n "$MISSING_DEPS" ]; then
    echo -e "\033[1;33m[Попередження]\033[0m Відсутні необхідні пакунки: $MISSING_DEPS"
    echo -e "Спроба автоматичного встановлення..."
    if command -v pacman &> /dev/null; then
        sudo pacman -S --needed --noconfirm base-devel meson ninja gtk4 curl mpv yt-dlp || true
    elif command -v apt-get &> /dev/null; then
        sudo apt-get update && sudo apt-get install -y build-essential meson ninja-build libgtk-4-dev libcurl4-openssl-dev libmpv-dev yt-dlp || true
    elif command -v dnf &> /dev/null; then
        sudo dnf install -y gcc-c++ meson ninja-build gtk4-devel libcurl-devel mpv-libs-devel yt-dlp || true
    fi
fi

# 2. Збірка програми
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo -e "\033[1;34m==> Компіляція проекту...\033[0m"
if [ ! -d "build" ]; then
    meson setup build --buildtype=release
fi
ninja -C build

# 3. Встановлення бінарника
PREFIX_BIN="$HOME/.local/bin"
PREFIX_LIB="$HOME/.local/lib"
PREFIX_DATA="$HOME/.local/share"
mkdir -p "$PREFIX_BIN" "$PREFIX_LIB" "$PREFIX_DATA/applications" "$PREFIX_DATA/icons/hicolor/256x256/apps" "$PREFIX_DATA/andub"

echo -e "\033[1;34m==> Встановлення файлів у $HOME/.local/...\033[0m"
cp -f build/andub "$PREFIX_BIN/andub"
chmod +x "$PREFIX_BIN/andub"
ln -sf "$PREFIX_BIN/andub" "$PREFIX_BIN/anime-gui"
ln -sf "$PREFIX_BIN/andub" "$PREFIX_BIN/anime-tui"
# Only ship the bundled libmpv.so if there's no system one — copying it
# unconditionally would shadow a correctly-linked system libmpv for anyone
# with ~/.local/lib on their LD_LIBRARY_PATH, reintroducing the exact
# "wrong libmpv ABI" crash this script is supposed to avoid.
if ! pkg-config --exists mpv; then
    cp -d lib/libmpv.so* "$PREFIX_LIB/" 2>/dev/null || true
else
    rm -f "$PREFIX_LIB"/libmpv.so*
fi

# Копіювання ресурсів
cp -f resources/style.css "$PREFIX_DATA/andub/style.css"
rm -rf "$PREFIX_DATA/andub/shaders"
cp -r resources/shaders "$PREFIX_DATA/andub/shaders"
if [ -f "resources/anime-gui.png" ]; then
    cp -f resources/anime-gui.png "$PREFIX_DATA/icons/hicolor/256x256/apps/andub.png"
elif [ -f "desktop/anime-gui.png" ]; then
    cp -f desktop/anime-gui.png "$PREFIX_DATA/icons/hicolor/256x256/apps/andub.png"
fi

# Встановлення .desktop ярлика
if [ -f "desktop/andub.desktop" ]; then
    sed -e "s|Exec=andub|Exec=$PREFIX_BIN/andub|g" desktop/andub.desktop > "$PREFIX_DATA/applications/andub.desktop"
    chmod +x "$PREFIX_DATA/applications/andub.desktop"
elif [ -f "desktop/anime-gui.desktop" ]; then
    sed -e "s|Exec=anime-gui|Exec=$PREFIX_BIN/andub|g" desktop/anime-gui.desktop > "$PREFIX_DATA/applications/andub.desktop"
    chmod +x "$PREFIX_DATA/applications/andub.desktop"
fi

if command -v update-desktop-database &> /dev/null; then
    update-desktop-database "$PREFIX_DATA/applications" || true
fi

echo -e "\033[1;32m[Готово!]\033[0m Andub успішно встановлено!"
echo -e "Додаток доступний у меню програм або через термінал командою: \033[1;36mandub\033[0m (або \033[1;36manime-gui\033[0m / \033[1;36manime-tui\033[0m)"
