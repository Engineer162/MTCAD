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

## Notes

- By default, the GUI links against the kernel import library. At runtime, `mtcad_kernel.dll` must be next to `mtcad_gui.exe` (or on `PATH`).
