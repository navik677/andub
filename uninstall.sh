#!/usr/bin/env bash
set -e

echo -e "\033[1;36m==> Видалення Andub...\033[0m"

PREFIX_BIN="$HOME/.local/bin"
PREFIX_DATA="$HOME/.local/share"

rm -f "$PREFIX_BIN/andub" "$PREFIX_BIN/anime-gui" "$PREFIX_BIN/anime-tui"
rm -f "$PREFIX_DATA/applications/andub.desktop" "$PREFIX_DATA/applications/anime-gui.desktop"
rm -f "$PREFIX_DATA"/icons/hicolor/*/apps/andub.png "$PREFIX_DATA"/icons/hicolor/*/apps/andub.svg
rm -f "$PREFIX_DATA/icons/hicolor/256x256/apps/anime-gui.png"
rm -rf "$PREFIX_DATA/andub" "$PREFIX_DATA/anime-gui"

# Кеш обкладинок (налаштування, улюблені та історія в ~/.config/anime-gui залишаються)
rm -rf "$HOME/.cache/anime-gui"

command -v update-desktop-database &>/dev/null && update-desktop-database "$PREFIX_DATA/applications" || true

echo -e "\033[1;32m[Готово!]\033[0m Andub видалено. Улюблені та історія збережені в ~/.config/anime-gui."
