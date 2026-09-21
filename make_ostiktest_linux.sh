#!/usr/bin/env bash
set -euo pipefail

if [ ! -d .git ]; then
    echo "Run this script from the root of the andub repository."
    exit 1
fi

git switch main
git pull --ff-only
git switch -c ostiktest

# Linux-only branch: remove platform-specific and Fedora-bundled artifacts.
git rm -rf android-tv 2>/dev/null || true
git rm -f scripts/build_windows.sh scripts/make_msi.py 2>/dev/null || true
git rm -f resources/andub.ico resources/andub.rc 2>/dev/null || true
git rm -rf lib 2>/dev/null || true

cat > meson.build <<'EOF'
project(
  'andub',
  'cpp',
  version: '1.1.0-linux',
  default_options: [
    'cpp_std=c++20',
    'warning_level=2',
    'buildtype=release',
  ],
)

gtk4_dep = dependency('gtk4', version: '>=4.6.0', required: true)
threads_dep = dependency('threads')
curl_dep = dependency('libcurl', required: true)
mpv_dep = dependency('mpv', required: true)

sources = files(
  'src/main.cpp',
  'src/application.cpp',
  'src/utils/http_client.cpp',
  'src/utils/kodik_resolver.cpp',
  'src/services/image_cache.cpp',
  'src/services/player_service.cpp',
  'src/services/history_manager.cpp',
  'src/services/favorites_manager.cpp',
  'src/services/download_service.cpp',
  'src/services/theme_manager.cpp',
  'src/providers/anilibria_provider.cpp',
  'src/providers/dreamcast_provider.cpp',
  'src/providers/shizaproject_provider.cpp',
  'src/providers/anibaza_provider.cpp',
  'src/providers/anidub_provider.cpp',
  'src/providers/anitube_provider.cpp',
  'src/providers/animevost_provider.cpp',
  'src/providers/anistar_provider.cpp',
  'src/ui/anime_card.cpp',
  'src/ui/details_view.cpp',
  'src/ui/player_view.cpp',
  'src/ui/downloads_view.cpp',
  'src/ui/main_window.cpp',
)

inc_dirs = include_directories('src', 'include')

executable(
  'andub',
  sources,
  include_directories: inc_dirs,
  dependencies: [
    gtk4_dep,
    threads_dep,
    curl_dep,
    mpv_dep,
  ],
  cpp_args: ['-DHAVE_LIBCURL'],
  install: true,
)

install_data(
  'resources/style.css',
  install_dir: get_option('datadir') / 'andub',
)

install_data(
  'resources/anime-gui.png',
  install_dir: get_option('datadir') / 'icons/hicolor/256x256/apps',
  rename: 'andub.png',
)

install_data(
  'desktop/andub.desktop',
  install_dir: get_option('datadir') / 'applications',
)
EOF

cat > build.sh <<'EOF'
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
EOF
chmod +x build.sh

cat > install.sh <<'EOF'
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
EOF
chmod +x install.sh

cat > uninstall.sh <<'EOF'
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
EOF
chmod +x uninstall.sh

cat > scripts/build_appimage.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

./build.sh

if ! command -v linuxdeploy >/dev/null 2>&1; then
    echo "linuxdeploy was not found."
    echo "Install linuxdeploy, then run this script again."
    exit 1
fi

if ! command -v appimagetool >/dev/null 2>&1; then
    echo "appimagetool was not found."
    echo "Install appimagetool, then run this script again."
    exit 1
fi

APPDIR="${ROOT_DIR}/dist/Andub.AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR"

DESTDIR="$APPDIR" meson install -C build

linuxdeploy \
    --appdir "$APPDIR" \
    --desktop-file desktop/andub.desktop \
    --icon-file resources/anime-gui.png \
    --executable build/andub

mkdir -p dist
ARCH=x86_64 appimagetool "$APPDIR" "dist/Andub-x86_64.AppImage"

echo "Created dist/Andub-x86_64.AppImage"
EOF
chmod +x scripts/build_appimage.sh

cat > .github/workflows/build-release.yml <<'EOF'
name: Linux Build

on:
  push:
    branches: ["main", "ostiktest"]
  pull_request:
    branches: ["main", "ostiktest"]
  workflow_dispatch:

jobs:
  ubuntu-build:
    runs-on: ubuntu-24.04

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential \
            meson \
            ninja-build \
            pkg-config \
            libgtk-4-dev \
            libcurl4-openssl-dev \
            libmpv-dev \
            mpv \
            yt-dlp \
            libnotify-bin

      - name: Configure
        run: meson setup build --buildtype=release

      - name: Build
        run: meson compile -C build

      - name: Run tests if configured
        run: |
          if meson test -C build --list | grep -q .; then
            meson test -C build --print-errorlogs
          fi
EOF

cat > README.md <<'EOF'
# Andub — Linux build

Native C++20 / GTK4 desktop anime client.

This branch is Linux-only and targets Ubuntu/Debian-style systems. Fedora-specific
bundled libraries, RPM-based Windows tooling, Android TV sources and Windows
packaging are intentionally excluded.

## Requirements

Ubuntu 24.04 / Debian-family packages:

```bash
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
  libnotify-bin
```

The application uses the system libmpv and the system mpv executable. No bundled
Fedora-built libmpv is used.

## Build

```bash
./build.sh
./build/andub
```

Or manually:

```bash
meson setup build --buildtype=release
meson compile -C build
./build/andub
```

## Install for the current user

```bash
./install.sh
```

Installed files go under `~/.local`.

Run:

```bash
andub
```

If `~/.local/bin` is not in `PATH`:

```bash
~/.local/bin/andub
```

## Uninstall

```bash
./uninstall.sh
```

## Main dependencies

- C++20 compiler
- GTK4
- libcurl
- libmpv
- mpv
- yt-dlp
- libnotify / notify-send
- Meson + Ninja

## Linux-specific changes

- removed bundled Fedora-built `libmpv.so*`;
- removed `/usr/lib64` and repository-local libmpv fallback logic;
- removed Fedora/RPM-specific Windows build tooling;
- removed Windows MSI/ZIP packaging from this branch;
- removed Android TV project from this branch;
- Meson requires the distro-provided `mpv` development package;
- build/install scripts use Ubuntu/Debian package names;
- install follows the XDG-style `~/.local` layout;
- GitHub CI builds on Ubuntu 24.04.
EOF

git add -A
git commit -m "Rewrite project as Linux-only Ubuntu build

Co-authored-by: ChatGPT <noreply@openai.com>"

echo
echo "Created branch: ostiktest"
echo "Push it with:"
echo "  git push -u origin ostiktest"
