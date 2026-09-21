#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

missing=()

command -v g++ >/dev/null 2>&1 || missing+=("g++")
command -v meson >/dev/null 2>&1 || missing+=("meson")
command -v ninja >/dev/null 2>&1 || missing+=("ninja-build")
command -v pkg-config >/dev/null 2>&1 || missing+=("pkg-config")
pkg-config --exists gtk4 2>/dev/null || missing+=("libgtk-4-dev")
pkg-config --exists libcurl 2>/dev/null || missing+=("libcurl4-openssl-dev")
pkg-config --exists mpv 2>/dev/null || missing+=("libmpv-dev")
command -v mpv >/dev/null 2>&1 || missing+=("mpv")
command -v yt-dlp >/dev/null 2>&1 || missing+=("yt-dlp")
command -v notify-send >/dev/null 2>&1 || missing+=("libnotify-bin")

if ((${#missing[@]})); then
    printf 'Missing Ubuntu/Debian packages:\n  %s\n' "${missing[*]}"
    echo
    echo "Install them with:"
    echo "sudo apt update && sudo apt install -y build-essential meson ninja-build pkg-config libgtk-4-dev libcurl4-openssl-dev libmpv-dev mpv yt-dlp libnotify-bin"
    exit 1
fi

if [ -d build ]; then
    meson setup --reconfigure build --buildtype=release
else
    meson setup build --buildtype=release
fi

meson compile -C build

echo
echo "Built successfully:"
echo "  ./build/andub"
