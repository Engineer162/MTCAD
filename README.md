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

## Build (Windows, CMake)

1. Clone repository:

```bash
git clone --recursive https://github.com/Engineer162/MTCAD.git
```

2. Install vulkan:
 
```bash
https://vulkan.lunarg.com/sdk/home
```
   
3. Configure:

```bash
cmake -S . -B build
```

4. Build:

```bash
cmake --build build --config Release
```

## Build from VS Code (Ctrl+Shift+P)

Use Command Palette and run one of:

- `Tasks: Run Build Task` to run the default task (`cmake: build all`)
- `Tasks: Run Task` and pick `cmake: build kernel` or `cmake: build gui`

The tasks are defined in `.vscode/tasks.json` and will configure CMake automatically before building.

## Notes

- By default, the GUI links against the kernel import library. At runtime, `mtcad_kernel.dll` must be next to `mtcad_gui.exe` (or on `PATH`).
