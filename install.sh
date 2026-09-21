#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

sudo apt update
sudo apt install -y \
    build-essential \
    meson \
    ninja-build \
    pkg-config \
    libgtk-4-dev \
    libcurl4-openssl-dev \
    libmpv-dev \
    mpv \
    yt-dlp \
    libnotify-bin \
    desktop-file-utils

./build.sh

PREFIX="${HOME}/.local"

meson install -C build --destdir "${ROOT_DIR}/.install-root"

INSTALL_ROOT="${ROOT_DIR}/.install-root${PREFIX}"

mkdir -p "${PREFIX}/bin"
mkdir -p "${PREFIX}/share/applications"
mkdir -p "${PREFIX}/share/icons/hicolor/256x256/apps"
mkdir -p "${PREFIX}/share/andub"

cp -f "${INSTALL_ROOT}/bin/andub" "${PREFIX}/bin/andub"
cp -f "${INSTALL_ROOT}/share/andub/style.css" "${PREFIX}/share/andub/style.css"
cp -f "${INSTALL_ROOT}/share/icons/hicolor/256x256/apps/andub.png" \
      "${PREFIX}/share/icons/hicolor/256x256/apps/andub.png"
cp -f "${INSTALL_ROOT}/share/applications/andub.desktop" \
      "${PREFIX}/share/applications/andub.desktop"

rm -rf "${ROOT_DIR}/.install-root"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${PREFIX}/share/applications" || true
fi

echo
echo "Andub installed."
echo "Run:"
echo "  ~/.local/bin/andub"
