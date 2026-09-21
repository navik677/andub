#!/usr/bin/env bash
set -euo pipefail

PREFIX="${HOME}/.local"

rm -f "${PREFIX}/bin/andub"
rm -f "${PREFIX}/share/applications/andub.desktop"
rm -f "${PREFIX}/share/icons/hicolor/256x256/apps/andub.png"
rm -rf "${PREFIX}/share/andub"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${PREFIX}/share/applications" || true
fi

echo "Andub removed."
