# Debian Build and Package

## Prerequisites

Install the development packages needed by the build:

```bash
sudo apt install libvulkan-dev libx11-dev libxext-dev libxrandr-dev libxrender-dev libxfixes-dev libxcursor-dev libxi-dev libxss-dev libxkbcommon-dev libxkbcommon-x11-dev libwayland-dev wayland-protocols libegl1-mesa-dev libgl1-mesa-dev
```

## CMake

Install CMake

```bash
sudo apt install cmake
```

## Build and Create the .deb

1. Clone the repository:

```bash
git clone --recursive https://github.com/Engineer162/MTCAD.git
cd MTCAD
```

2. Configure, build, and package for Debian:

```bash
cmake -DCPACK_GENERATOR=DEB -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target package
```

The generated `.deb` is written to the `build/` directory (for example `build/mtcad_0.2.1_amd64.deb`).

## Install

Install the .deb with:

```bash
sudo dpkg -i build/mtcad_0.2.1_amd64.deb
```

Then launch the app with:

```bash
mtcad
```

## Notes

- The Debian package installs the GUI, kernel library, launcher, assets, and default ImGui config under `/usr/lib/mtcad/`.
- User settings are stored in the per-user config directory returned by `SDL_GetPrefPath("MTCAD", "MTCAD")`.
