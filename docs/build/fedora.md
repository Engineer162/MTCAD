
# Fedora / RPM Build and Package

## Prerequisites

Install the development packages needed by the build (Fedora/RHEL/CentOS):

```bash
sudo dnf install vulkan-devel libX11-devel libXrandr-devel libXcursor-devel libXfixes-devel libXi-devel libXScrnsaver-devel libxkbcommon-devel wayland-devel wayland-protocols mesa-libEGL-devel mesa-libGL-devel cmake
```

## Build and Create the .rpm

1. Clone the repository:

```bash
git clone --recursive https://github.com/Engineer162/MTCAD.git
cd MTCAD
```

2. Configure, build, and package for RPM:

```bash
cmake -DCPACK_GENERATOR=RPM -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target package
```

The generated `.rpm` is written to the `build/` directory (for example `build/mtcad-0.2.1-1.x86_64.rpm`).

## Install

Install the package with:

```bash
sudo dnf install ./build/mtcad-0.2.1-1.x86_64.rpm
```

Or using `rpm`:

```bash
sudo rpm -i ./build/mtcad-0.2.1-1.x86_64.rpm
```

Then launch the app with:

```bash
mtcad
```

## Notes

- The RPM installs the GUI, kernel library, launcher, assets, and default ImGui config under the system prefixes (e.g., `/usr/lib/mtcad` and `/usr/share/mtcad/assets`).
- The package assumes system Vulkan and X11/Wayland development libraries are present at build time; SDL3 is linked statically.
