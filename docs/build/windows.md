# Windows Build

1. Clone the repository:

```bash
git clone --recursive https://github.com/Engineer162/MTCAD.git
```
2. Install CMake from:

```bash
https://cmake.org/download/
```

3. Install the Vulkan SDK from:

```bash
https://vulkan.lunarg.com/sdk/home
```

4. Configure:

```bash
cmake -S . -B build
```

5. Build:

```bash
cmake --build build --config Release
```

## Visual Studio Code

Use the Command Palette and run one of:

- `Tasks: Run Build Task` to run the default task (`cmake: build all`)
- `Tasks: Run Task` and pick `cmake: build kernel` or `cmake: build gui`

The tasks in `.vscode/tasks.json` configure CMake automatically before building.

## Notes

- By default, the GUI links against the kernel import library. At runtime, `mtcad_kernel.dll` must be next to `mtcad_gui.exe` or available on `PATH`.
