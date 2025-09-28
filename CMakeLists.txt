cmake_minimum_required(VERSION 3.16)
project(mame_synth VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# MAME build configuration
set(MAME_DIR ${CMAKE_SOURCE_DIR}/mame)
set(MAME_SRC_DIR ${MAME_DIR}/src)

# Add MAME include directories
include_directories(
    ${MAME_SRC_DIR}
    ${MAME_SRC_DIR}/emu
    ${MAME_SRC_DIR}/lib
    ${MAME_SRC_DIR}/lib/util
    ${MAME_SRC_DIR}/osd
    ${MAME_SRC_DIR}/devices
)

# Define preprocessor flags to match MAME's build
add_definitions(
    -DLSB_FIRST=1
    -DFLAC__NO_DLL
    -DPTR64=1
    -DMAME_DEBUG=0
    -DCRLF=2
)

# For now, create a minimal build without MAME dependencies
# This establishes the build system structure

# Our project sources
set(PROJECT_SOURCES
    src/main.cpp
    src/music_parser.cpp
    src/audio_device.cpp
    src/register_mapping.cpp
    src/mame_integration.cpp
    src/mame_core/mame_minimal.cpp
    src/mame_core/minimal_nesapu.cpp
    src/nes_note_mapping.cpp
    src/audio_stream.cpp
    src/nes_audio_mixer.cpp
    src/nes_sequencer.cpp
    src/nes_playback_engine.cpp
    src/comprehensive_file_support.cpp
    src/nes_cli.cpp
    src/nes_config.cpp
    src/nes_realtime_control.cpp
    src/nes_channel_assignment.cpp
    src/nes_channel_assignment_cli.cpp
)

# Create main executable
add_executable(mame_synth ${PROJECT_SOURCES})

# Find XML library for MusicXML support
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PUGIXML pugixml)
endif()

if(NOT PUGIXML_FOUND)
    # Try to find pugixml manually
    find_path(PUGIXML_INCLUDE_DIR pugixml.hpp
        PATHS /usr/include /usr/local/include /opt/local/include)
    find_library(PUGIXML_LIBRARY pugixml
        PATHS /usr/lib /usr/local/lib /opt/local/lib)

    if(PUGIXML_INCLUDE_DIR AND PUGIXML_LIBRARY)
        set(PUGIXML_FOUND TRUE)
        set(PUGIXML_INCLUDE_DIRS ${PUGIXML_INCLUDE_DIR})
        set(PUGIXML_LIBRARIES ${PUGIXML_LIBRARY})
    endif()
endif()

if(PUGIXML_FOUND)
    target_include_directories(mame_synth PRIVATE ${PUGIXML_INCLUDE_DIRS})
    target_link_libraries(mame_synth ${PUGIXML_LIBRARIES})
    target_compile_definitions(mame_synth PRIVATE HAVE_PUGIXML)
    message(STATUS "Found pugixml: ${PUGIXML_LIBRARIES}")
else()
    message(WARNING "pugixml not found - MusicXML support will be disabled")
    message(STATUS "To install pugixml on Ubuntu/Debian: sudo apt-get install libpugixml-dev")
    message(STATUS "To install pugixml on macOS: brew install pugixml")
endif()

# Find and link audio libraries
if(UNIX AND NOT APPLE)
    # Linux - use ALSA
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA alsa)
    if(ALSA_FOUND)
        target_include_directories(mame_synth PRIVATE ${ALSA_INCLUDE_DIRS})
        target_link_libraries(mame_synth ${ALSA_LIBRARIES})
        target_compile_definitions(mame_synth PRIVATE HAVE_ALSA)
        message(STATUS "Found ALSA: ${ALSA_LIBRARIES}")
    else()
        message(WARNING "ALSA not found - audio will use file output only")
    endif()
elseif(WIN32)
    # Windows - use DirectSound
    target_link_libraries(mame_synth dsound)
    target_compile_definitions(mame_synth PRIVATE HAVE_DIRECTSOUND)
endif()

# Compiler-specific flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(mame_synth PRIVATE -Wall -Wextra -O2)
endif()

# Create build directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/src)

# Add tests subdirectory
add_subdirectory(tests)

# Add tools subdirectory
add_subdirectory(tools)