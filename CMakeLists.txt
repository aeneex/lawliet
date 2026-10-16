cmake_minimum_required(VERSION 3.22)

project(Lawliet
    VERSION 0.1.0
    DESCRIPTION "Lightweight real-time dual-pitch voice changer"
    HOMEPAGE_URL "https://github.com/ixubux/Lawliet"
    LANGUAGES CXX C
)

if(NOT WIN32)
    message(FATAL_ERROR "Lawliet is Windows-only")
endif()

# ── Language standard ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── Output directories ─────────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ── Options ────────────────────────────────────────────────────────────────────
option(LAWLIET_TESTS "Build unit tests" ON)

# ── Compiler warnings & Optimization flags ─────────────────────────────────────
add_library(lawliet_compiler_flags INTERFACE)
target_compile_options(lawliet_compiler_flags INTERFACE
    -Wall -Wextra
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O2 -flto>
)

# ── External dependencies (Vendored, offline) ──────────────────────────────────
add_subdirectory(external)

# ── Core application ───────────────────────────────────────────────────────────
add_subdirectory(src)

# ── Tests ──────────────────────────────────────────────────────────────────────
if(LAWLIET_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Summary ────────────────────────────────────────────────────────────────────
message(STATUS "[Lawliet] Version       : ${PROJECT_VERSION}")
message(STATUS "[Lawliet] Build type    : ${CMAKE_BUILD_TYPE}")
message(STATUS "[Lawliet] Build tests   : ${LAWLIET_TESTS}")
