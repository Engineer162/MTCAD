# Debian Build and Package

## Prerequisites

Install the development packages needed by the build:

```bash
sudo apt install libvulkan-dev libx11-dev libxext-dev libxrandr-dev libxrender-dev libxfixes-dev libxcursor-dev libxi-dev libxss-dev libxkbcommon-dev libxkbcommon-x11-dev libwayland-dev wayland-protocols libegl1-mesa-dev libgl1-mesa-dev
```

## Build

1. Clone the repository:

```bash
git clone --recursive https://github.com/Engineer162/MTCAD.git
```

2. Configure and build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Create the .deb

Build the package target:

```bash
cmake --build build --target package
```

The generated package is written to `installer/`.

## Install

Install the package with:

```bash
sudo dpkg -i installer/mtcad_0.1.0_amd64.deb
```

Then launch the app with:

```bash
mtcad
```

## Notes

- The Debian package installs the GUI, kernel library, launcher, assets, and default ImGui config under `/usr/lib/mtcad/`.
- User settings are stored in the per-user config directory returned by `SDL_GetPrefPath("MTCAD", "MTCAD")`.
