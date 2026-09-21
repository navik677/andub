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
