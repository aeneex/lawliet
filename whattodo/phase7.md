# Phase 7: Trim Dependencies and Build System

**Goal:** a fresh clone configures and builds offline into one small, static, GUI-subsystem exe.

## Vendoring

All dependencies live as plain files in `external/`. No submodules, no `FetchContent`, no network at configure time. Convert any submodule or fetched dependency (see your phase 1 notes on GLFW) into plain vendored files, and delete any nested `.git` file or folder inside `external/`.

Keep only what the build needs, and keep each library's license file.

- **miniaudio:** just `miniaudio.h`. Compile it in one translation unit with:
  ```cpp
  #define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
  #define MA_ENABLE_WASAPI
  #define MA_NO_DECODING
  #define MA_NO_ENCODING
  #define MA_NO_GENERATION
  #define MA_NO_RESOURCE_MANAGER
  #define MA_NO_NODE_GRAPH
  #define MA_NO_ENGINE
  ```
  Drop any define that stops it compiling and log which.
- **SoundTouch:** only the library sources and headers the pitch shifter uses. Delete `soundstretch`, the DLL wrapper, BPM detection, WAV helpers, docs, tests and build files for other toolchains. Add `external/soundtouch/` license text. SoundTouch is LGPL, so it stays a clearly separate, replaceable component.
- **Dear ImGui:** core files (`imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`, headers, `imstb_*`) plus only `imgui_impl_glfw` and `imgui_impl_opengl3` (and its loader header). Delete `examples/`, `docs/`, `misc/`, other backends.
- **GLFW:** source folders needed to build it as a subproject, with `GLFW_BUILD_EXAMPLES`, `GLFW_BUILD_TESTS`, `GLFW_BUILD_DOCS` and `GLFW_INSTALL` all OFF.

## CMake

- One executable `lawliet` as a Windows GUI subsystem app (`WIN32` / `-mwindows`), C++17.
- One optional test executable `lawliet_tests` behind `option(LAWLIET_TESTS "..." ON)`.
- Release flags: `-O2`, `-s`. Try `-flto` and keep it only if it works and shrinks the exe.
- MinGW static linking: `-static -static-libgcc -static-libstdc++`.
- Build clean with `-Wall -Wextra`. Fix warnings in our code, not in `external/`.
- Remove MSVC/Clang branches and any dead options.
- `CMakePresets.json`: `windows-debug` and `windows-release` only (Ninja).
- Delete `rebuild.bat` and `run.bat`. Add a single `build.bat` that configures and builds `windows-release` and prints the exe path.

## Verify

1. Delete the build folder and build `windows-release` from scratch.
2. List the exe's imports: `objdump -p build\windows-release\bin\lawliet.exe | findstr "DLL Name"`. Only system DLLs should appear (KERNEL32, USER32, GDI32, SHELL32, OPENGL32, OLE32, WINMM, and similar). No `libstdc++`, `libgcc`, `libwinpthread`.
3. Record the final exe size next to the phase 1 baseline in `NOTES.md`.
4. Confirm no network is needed: configure with the network disabled, or grep CMake for `FetchContent`, `git`, `http`.

## Done when

- Release exe is self-contained and the build is offline-capable.
- Tests still build and pass in debug.
- `external/` holds only what's used, plus licenses.
