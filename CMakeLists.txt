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
)

# Create main executable
add_executable(mame_synth ${PROJECT_SOURCES})

# Compiler-specific flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(mame_synth PRIVATE -Wall -Wextra -O2)
endif()

# Create build directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/src)