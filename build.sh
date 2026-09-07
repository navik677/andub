#!/usr/bin/env bash
set -e

echo -e "\033[1;36m==> Збірка Anime GUI (GTK4 + C++20)...\033[0m"

if ! pkg-config --exists gtk4; then
    echo -e "\033[1;33m[Попередження]\033[0m Пакети розробки GTK4 не знайдено в системі."
    echo "Для збірки встановіть необхідні залежності:"
    echo "  Fedora:        sudo dnf install -y gtk4-devel libadwaita-devel libcurl-devel"
    echo "  Ubuntu/Debian: sudo apt install -y libgtk-4-dev libadwaita-1-dev libcurl4-openssl-dev"
    echo "  Arch Linux:    sudo pacman -S gtk4 libadwaita curl"
    exit 1
fi

BUILD_DIR="build"
if [ ! -d "$BUILD_DIR" ]; then
    meson setup "$BUILD_DIR" --buildtype=release
else
    meson setup --reconfigure "$BUILD_DIR"
fi

ninja -C "$BUILD_DIR"

echo ""
echo -e "\033[1;32m[✓] Збірка успішно завершена!\033[0m Запустити додаток:"
echo -e "    \033[1;36m./build/anime-gui\033[0m"
