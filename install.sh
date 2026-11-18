#!/usr/bin/env bash
set -e

echo -e "\033[1;36m==> Встановлення Andub (Qt 6)...\033[0m"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

have_qt() {
    for d in /usr/lib64/cmake/Qt6Quick /usr/lib/cmake/Qt6Quick /usr/lib/x86_64-linux-gnu/cmake/Qt6Quick; do
        [ -d "$d" ] && return 0
    done
    return 1
}

# 1. Залежності
if ! command -v cmake &>/dev/null || ! have_qt || ! pkg-config --exists mpv 2>/dev/null; then
    echo -e "\033[1;33m==> Встановлення залежностей...\033[0m"
    if command -v pacman &>/dev/null; then
        sudo pacman -S --needed --noconfirm base-devel cmake ninja qt6-base qt6-declarative qt6-svg \
            qt6-imageformats mpv curl yt-dlp
    elif command -v dnf &>/dev/null; then
        sudo dnf install -y gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtdeclarative-devel \
            qt6-qtsvg qt6-qtimageformats mpv-devel libcurl-devel mpv yt-dlp
    elif command -v apt-get &>/dev/null; then
        sudo apt-get update
        sudo apt-get install -y build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev \
            qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-templates \
            qml6-module-qtquick-layouts qml6-module-qtquick-effects qml6-module-qtquick-shapes \
            qml6-module-qtqml-workerscript qt6-image-formats-plugins libqt6svg6 \
            libmpv-dev libcurl4-openssl-dev mpv yt-dlp
    else
        echo "Невідомий пакетний менеджер. Встановіть вручну: CMake, Qt 6.6+ (Quick, QuickControls2), libmpv, libcurl."
    fi
fi

# 2. Збірка та встановлення в ~/.local
GENERATOR=()
command -v ninja &>/dev/null && GENERATOR=(-G Ninja)
cmake -S . -B build "${GENERATOR[@]}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build --parallel
cmake --install build

PREFIX_BIN="$HOME/.local/bin"
PREFIX_DATA="$HOME/.local/share"
ln -sf "$PREFIX_BIN/andub" "$PREFIX_BIN/anime-gui"

# Old GTK installs shipped a stylesheet that is no longer used
rm -f "$PREFIX_DATA/andub/style.css"

if [ -f "$PREFIX_DATA/applications/andub.desktop" ]; then
    sed -i "s|^Exec=andub|Exec=$PREFIX_BIN/andub|" "$PREFIX_DATA/applications/andub.desktop"
fi
# Refresh launcher caches (rofi reads .desktop files directly; KDE keeps its own index)
command -v update-desktop-database &>/dev/null && update-desktop-database "$PREFIX_DATA/applications" || true
command -v gtk-update-icon-cache &>/dev/null && gtk-update-icon-cache -q -t "$PREFIX_DATA/icons/hicolor" || true
command -v kbuildsycoca6 &>/dev/null && kbuildsycoca6 --noincremental >/dev/null 2>&1 || true

echo -e "\033[1;32m[Готово!]\033[0m Andub встановлено. Запуск: \033[1;36mandub\033[0m або з меню програм."
