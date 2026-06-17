# MTCAD

Modular CAD architecture with:

- `kernel`: C geometric kernel, built as a shared library (`mtcad_kernel.dll` on Windows) using libuv as core library
- `gui`: C++ desktop frontend (`mtcad_gui.exe`) using ImGui + Vulkan + SDL3 + STB

## Why this split

Keeping the kernel in C behind a stable C API makes it reusable for:

- desktop GUI apps
- web backends (native service process)
- Server seperation
- language bindings later (Python, Rust, etc.)

## Core project layout

```text
MTCAD/
	CMakeLists.txt
	gui/
		CMakeLists.txt
		src/main.cpp
	kernel/
		CMakeLists.txt
		include/mtcad/kernel.h
		src/kernel.c
	third_party/
		SDL/
		imgui/
		libuv/
		stb/
```

## Build Guides

- [Windows build instructions](docs/build/windows.md)
- [Debian build and packaging instructions](docs/build/debian.md)
- [Fedora build and packaging instructions](docs/build/fedora.md)

Follow the build instructions for Fedora if you are using Red Hat Enterprise, CentOS, openSUSE, SUSE Linux Enterprise, AlmaLinux, Oracle Linux, Mageia, OpenMandriva Lx or others that use the .rpm package manager.
Follow the build instructions for Debian if you are using Ubuntu, Linux Mint or others that use the .deb package format