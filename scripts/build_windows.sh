#!/usr/bin/env bash
# Windows build + installer. Run in an MSYS2 UCRT64 shell (CI does the same; mpv
# is no longer packaged for the old MINGW64 environment):
#   pacman -S --needed zip mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,qt6-base,qt6-declarative,qt6-svg,qt6-imageformats,mpv,curl,nsis}
# Output:
#   dist/Andub-Setup-x86_64.exe     one-file installer
#   dist/Andub-Windows-x86_64.zip   portable folder
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
DIST="$ROOT_DIR/dist"
BUNDLE="$DIST/Andub-Windows-x86_64"
VERSION="$(sed -n 's/^project(andub VERSION \([0-9.]*\).*/\1/p' "$ROOT_DIR/CMakeLists.txt")"

cd "$ROOT_DIR"
echo "==> Building Andub $VERSION"
cmake -S . -B build_win -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build_win --parallel

rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/resources"
cp build_win/andub.exe "$BUNDLE/"
cp -r resources/shaders "$BUNDLE/resources/"

echo "==> windeployqt"
WINDEPLOYQT=""
PREFIX="${MINGW_PREFIX:-/ucrt64}"
for c in windeployqt6 windeployqt-qt6 windeployqt "$PREFIX/share/qt6/bin/windeployqt.exe"; do
    if command -v "$c" &>/dev/null; then WINDEPLOYQT="$c"; break; fi
done
[ -n "$WINDEPLOYQT" ] || { echo "windeployqt not found"; exit 1; }
"$WINDEPLOYQT" --release --qmldir qml --no-translations --no-system-d3d-compiler --no-opengl-sw "$BUNDLE/andub.exe"

# External mpv window ("Відкрити в mpv"); found next to andub.exe at runtime
if command -v mpv &>/dev/null; then cp "$(command -v mpv)" "$BUNDLE/"; fi

echo "==> Collecting runtime DLLs from $PREFIX"
# Repeat until no new DLLs appear: plugins (webp, svg, QML) pull in their own deps
while true; do
    added=0
    while read -r dll; do
        name="$(basename "$dll")"
        if [ ! -f "$BUNDLE/$name" ]; then cp "$dll" "$BUNDLE/"; added=1; fi
    done < <(find "$BUNDLE" -type f \( -iname '*.dll' -o -iname '*.exe' \) -print0 \
             | xargs -0 ldd 2>/dev/null | awk -v p="$PREFIX/" 'index($3, p) == 1 {print $3}' | sort -u)
    [ "$added" -eq 0 ] && break
done

cd "$DIST"
rm -f Andub-Windows-x86_64.zip Andub-Setup-x86_64.exe
zip -r -q Andub-Windows-x86_64.zip "$(basename "$BUNDLE")"

echo "==> Building installer"
makensis -V2 -DVERSION="$VERSION" -DBUNDLE="$(cygpath -w "$BUNDLE")" \
    -DOUTFILE="$(cygpath -w "$DIST/Andub-Setup-x86_64.exe")" "$(cygpath -w "$ROOT_DIR/installer/andub.nsi")"

ls -lh "$DIST/Andub-Setup-x86_64.exe" "$DIST/Andub-Windows-x86_64.zip"
