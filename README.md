# CG_TP_3 — Image Filter Demo

Small OpenGL/GLFW demo that loads a PNG and applies a mean (box) filter in a fullscreen quad. A simple on-screen button toggles between original and filtered, and a slider adjusts the filter radius.

## Build

```bash
cmake -S . -B build
cmake --build build
```

Requirements: OpenGL, GLEW, GLFW 3.3, GLM, libpng (found via CMake packages).

## Run

```bash
./build/CG_TP_3
```

Shaders are copied next to the binary after the build.

## Controls/UI

- Button (bottom-left): toggles filtered vs original view.
- Slider (next to button): drag to change mean filter radius (1–50). Filtered texture is cached per current radius to avoid re-rendering every frame.
- Esc: quit.

## Notes

- Filtering is implemented in `assets/shaders/filter.frag`. Radius is a uniform (`uRadius`), and the shader performs a box blur over a square kernel.
- Texture loading uses libpng (`src/TextureLoader.cpp`). Shaders and GL program management live in `src/ShaderProgram.*`.

