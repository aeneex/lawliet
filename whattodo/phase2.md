# Phase 2: Remove Non-Windows and Non-Cable Code

**Goal:** the project only supports Windows, and the only way audio reaches other apps is the virtual cable.

## Delete

- `platform/Linux/`, `platform/MacOS/`
- `platform/Windows/apo/`, `platform/Windows/driver/` (kernel driver, INF, APO)
- `platform/Windows/WindowsVirtualMicrophone.cpp` (named pipe / shared memory backend)
- `include/lawliet/platform/IVirtualMicrophone.hpp` and the whole `platform/` include folder if it ends up empty
- `src/core/AppController.cpp` and its header (headless CLI)
- Diagnostic tests: `test_audio_flow.cpp`, `test_engine_to_cable.cpp`, `test_virtual_mic.cpp` (wherever they live)

## Then

1. Read `platform/Windows/WindowsPlatformAudio.cpp`. If it only holds a small helper (COM init, device lookup), move that helper into `AudioEngine.cpp` and delete the file. If it's unused, delete it.
2. Remove every non-Windows branch: `__APPLE__`, `__linux__`, `#else` fallbacks, MSVC/Clang-only workarounds. Keep only the Windows path, unwrapped.
3. Remove every use of the removed virtual-mic interface from `AudioEngine` and `main.cpp`. `main.cpp` should just create the engine and run the GUI.
4. Remove all references to the shared buffer `LawlietVirtualAudioBuffer` and to a device called "Lawliet Microphone", in code, GUI text and tests.
5. CMake: drop the deleted sources/targets and platform conditionals. Add near the top:
   ```cmake
   if(NOT WIN32)
     message(FATAL_ERROR "Lawliet is Windows-only")
   endif()
   ```
6. Remove the deleted tests from `test_runner.cpp` and CMake.

## Verify

```powershell
Get-ChildItem -Recurse -Include *.cpp,*.hpp,*.h,*.txt,*.json,*.md |
  Select-String -Pattern "apple|__linux__|pipewire|pulse|coreaudio|IVirtualMicrophone|LawlietVirtualAudioBuffer|Lawliet Microphone|kernel|APO" -CaseSensitive:$false
```

Ignore hits inside `external/`. Everything else must be zero or justified in `NOTES.md`.

## Done when

- Project builds and remaining tests pass.
- Grep above is clean outside `external/`.
- `platform/` contains nothing (so it's gone).
