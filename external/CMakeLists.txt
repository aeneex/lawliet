# external/CMakeLists.txt
# Vendored dependencies (offline build, zero network, zero FetchContent).

cmake_minimum_required(VERSION 3.22)

# ── miniaudio ──────────────────────────────────────────────────────────────────
# Single-header audio I/O library. Public Domain / MIT-0.
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/miniaudio")

# ── GLFW ──────────────────────────────────────────────────────────────────────
# Window creation and input handling library. zlib/libpng license.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)

# ── Dear ImGui ─────────────────────────────────────────────────────────────────
# Immediate-mode GUI library. MIT License.
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)
add_library(imgui_core STATIC ${IMGUI_SOURCES})
target_include_directories(imgui_core PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)
target_compile_options(imgui_core PRIVATE -w)
target_link_libraries(imgui_core PUBLIC glfw)

# ── SoundTouch ─────────────────────────────────────────────────────────────────
# Pitch-shifting DSP library. LGPL v2.1.
# Linked as a shared library to comply with LGPL §6 (user can swap the lib).
set(SOUNDTOUCH_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/source/AAFilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/source/FIFOSampleBuffer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/source/FIRFilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/source/InterpolateCubic.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/source/InterpolateLinear.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/source/InterpolateShannon.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/source/RateTransposer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/source/SoundTouch.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/source/TDStretch.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/source/cpu_detect_x86.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/source/mmx_optimized.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/source/sse_optimized.cpp"
)
add_library(SoundTouch STATIC ${SOUNDTOUCH_SOURCES})
target_include_directories(SoundTouch PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/source"
)
target_compile_options(SoundTouch PRIVATE -w -O3)
