# tests/CMakeLists.txt
# Unit tests for lawliet_core.

add_executable(lawliet_tests
    test_settings.cpp
    test_dual_layer_mixer.cpp
    test_dsp.cpp
    test_audio_engine.cpp
    test_runner.cpp
)

target_link_libraries(lawliet_tests
    PRIVATE
        lawliet_core
        SoundTouch
)

# Target linker flags: static MinGW runtime
target_link_options(lawliet_tests PRIVATE
    -static
    -static-libgcc
    -static-libstdc++
)


add_test(NAME lawliet_tests COMMAND lawliet_tests)
