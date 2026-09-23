#!/usr/bin/env bash

set -e

echo -e "\033[1;36m==> Видалення Anime GUI...\033[0m"

PREFIX_BIN="$HOME/.local/bin"
PREFIX_DATA="$HOME/.local/share"

# 1. Видалення бінарників
rm -f "$PREFIX_BIN/anime-gui"
rm -f "$PREFIX_BIN/anime-tui"

# 2. Видалення desktop файлу та іконок
rm -f "$PREFIX_DATA/applications/anime-gui.desktop"
rm -f "$PREFIX_DATA/icons/hicolor/256x256/apps/anime-gui.png"
rm -rf "$PREFIX_DATA/anime-gui"
rm -rf "$PREFIX_DATA/andub/shaders"

# 3. Видалення кешу та налаштувань за бажанням
if [ -d "$HOME/.cache/anime-gui" ]; then
    rm -rf "$HOME/.cache/anime-gui"
fi

if command -v update-desktop-database &> /dev/null; then
    update-desktop-database "$PREFIX_DATA/applications" || true
fi

echo -e "\033[1;32m[Готово!]\033[0m Anime GUI успішно видалено з системи."
