# src/CMakeLists.txt
# Builds the lawliet_core static library and the lawliet GUI subsystem executable.

# ── lawliet_core static library ────────────────────────────────────────────────
add_library(lawliet_core STATIC
    audio/AudioEngine.cpp
    config/Settings.cpp
    dsp/DualLayerMixer.cpp
    dsp/SoundTouchPitchShifter.cpp
    gui/GuiController.cpp
)

target_include_directories(lawliet_core
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(lawliet_core
    PUBLIC imgui_core
    PUBLIC lawliet_compiler_flags
    PRIVATE miniaudio
    PRIVATE SoundTouch
    PRIVATE opengl32
    PRIVATE winmm
    PRIVATE ole32
    PRIVATE dsound
)

# ── Executable (WIN32 subsystem app) ──────────────────────────────────────────
add_executable(lawliet WIN32 main.cpp)
target_link_libraries(lawliet PRIVATE lawliet_core)

# Target linker flags: static MinGW runtime and GUI subsystem (-mwindows)
target_link_options(lawliet PRIVATE
    -mwindows
    -static
    -static-libgcc
    -static-libstdc++
)

# In release mode, strip symbol table and enable LTO
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(lawliet PRIVATE -s -flto)
endif()


install(TARGETS lawliet RUNTIME DESTINATION bin)
