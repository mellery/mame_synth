cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Find required packages for testing
find_package(PkgConfig REQUIRED)

# Use a simple testing framework (no external dependencies)
# We'll create our own minimal testing framework

# Include parent directories for access to source files
include_directories(../src)

# Test source files
set(TEST_SOURCES
    test_main.cpp
    test_music_parser.cpp
    test_audio_device.cpp
    test_register_mapping.cpp
    test_integration.cpp
    test_nes_note_mapping.cpp
    test_nes_integration.cpp
    test_cli_integration.cpp
    test_workflow_integration.cpp
    test_channel_assignment.cpp
    test_nes_config.cpp
    test_audio_stream.cpp
    test_nes_sequencer.cpp
    test_end_to_end_integration.cpp
)

# Source files to test (exclude main.cpp)
set(LIB_SOURCES
    ../src/machine_stub.cpp
    ../src/music_parser.cpp
    ../src/audio_device.cpp
    ../src/register_mapping.cpp
    ../src/mame_integration.cpp
    ../src/mame_core/mame_minimal.cpp
    ../src/mame_core/minimal_nesapu.cpp
    ../src/nes_note_mapping.cpp
    ../src/audio_stream.cpp
    ../src/nes_audio_mixer.cpp
    ../src/nes_sequencer.cpp
    ../src/nes_playback_engine.cpp
    ../src/comprehensive_file_support.cpp
    ../src/nes_realtime_control.cpp
    ../src/nes_config.cpp
    ../src/nes_channel_assignment.cpp
    ../src/nes_cli.cpp
    ../src/nes_channel_assignment_cli.cpp
)

# Create test executable
add_executable(mame_synth_tests ${TEST_SOURCES} ${LIB_SOURCES})

# Create MAME integration test executable
add_executable(mame_integration_tests
    test_mame_integration.cpp
    ../src/mame_integration.cpp
    ../src/mame_core/mame_minimal.cpp
    ../src/mame_core/minimal_nesapu.cpp
    ../src/nes_note_mapping.cpp
    ../src/audio_stream.cpp
    ../src/audio_device.cpp
    ../src/nes_audio_mixer.cpp
    ../src/nes_sequencer.cpp
    ../src/nes_playback_engine.cpp
    ../src/comprehensive_file_support.cpp
    ../src/nes_realtime_control.cpp
    ../src/nes_config.cpp
    ../src/music_parser.cpp
    ../src/nes_config.cpp
    ../src/register_mapping.cpp
    ../src/machine_stub.cpp
)

# Link audio libraries to tests as well
if(UNIX AND NOT APPLE)
    if(ALSA_FOUND)
        target_include_directories(mame_synth_tests PRIVATE ${ALSA_INCLUDE_DIRS})
        target_link_libraries(mame_synth_tests ${ALSA_LIBRARIES})
        target_compile_definitions(mame_synth_tests PRIVATE HAVE_ALSA)

        target_include_directories(mame_integration_tests PRIVATE ${ALSA_INCLUDE_DIRS})
        target_link_libraries(mame_integration_tests ${ALSA_LIBRARIES})
        target_compile_definitions(mame_integration_tests PRIVATE HAVE_ALSA)
    endif()
elseif(WIN32)
    target_link_libraries(mame_synth_tests dsound)
    target_compile_definitions(mame_synth_tests PRIVATE HAVE_DIRECTSOUND)

    target_link_libraries(mame_integration_tests dsound)
    target_compile_definitions(mame_integration_tests PRIVATE HAVE_DIRECTSOUND)
endif()

# Compiler-specific flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(mame_synth_tests PRIVATE -Wall -Wextra -O2)
    target_compile_options(mame_integration_tests PRIVATE -Wall -Wextra -O2)
endif()

# Add tests
add_test(NAME music_parser_tests COMMAND mame_synth_tests --test-suite=music_parser)
add_test(NAME audio_device_tests COMMAND mame_synth_tests --test-suite=audio_device)
add_test(NAME register_mapping_tests COMMAND mame_synth_tests --test-suite=register_mapping)
add_test(NAME integration_tests COMMAND mame_synth_tests --test-suite=integration)
add_test(NAME nes_integration_tests COMMAND mame_synth_tests --test-suite=nes_integration)
add_test(NAME nes_note_mapping_tests COMMAND mame_synth_tests --test-suite=nes_note_mapping)
add_test(NAME cli_integration_tests COMMAND mame_synth_tests --test-suite=cli_integration)
add_test(NAME workflow_integration_tests COMMAND mame_synth_tests --test-suite=workflow_integration)
add_test(NAME channel_assignment_tests COMMAND mame_synth_tests --test-suite=channel_assignment)
add_test(NAME nes_config_tests COMMAND mame_synth_tests --test-suite=nes_config)
add_test(NAME audio_stream_tests COMMAND mame_synth_tests --test-suite=audio_stream)
add_test(NAME nes_sequencer_tests COMMAND mame_synth_tests --test-suite=nes_sequencer)
add_test(NAME end_to_end_tests COMMAND mame_synth_tests --test-suite=end_to_end)
add_test(NAME all_tests COMMAND mame_synth_tests --test-suite=all)
add_test(NAME mame_integration_tests COMMAND mame_integration_tests)

# Add comprehensive test runner
add_executable(comprehensive_test_runner test_runner_comprehensive.cpp)
target_compile_options(comprehensive_test_runner PRIVATE -Wall -Wextra -O2)

# Performance and stress tests
add_test(NAME performance_tests COMMAND comprehensive_test_runner --performance)
add_test(NAME stress_tests COMMAND comprehensive_test_runner --stress)
add_test(NAME comprehensive_tests COMMAND comprehensive_test_runner --all)