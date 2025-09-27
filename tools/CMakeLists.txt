cmake_minimum_required(VERSION 3.16)

# Tools for NES Synthesizer development
# These utilities help generate example files and perform development tasks

# Example generator tool
add_executable(generate_nes_examples generate_nes_examples.cpp)

# Set compiler flags for tools
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(generate_nes_examples PRIVATE -Wall -Wextra -O2)
endif()

# Custom target to generate examples
add_custom_target(generate_examples
    COMMAND generate_nes_examples
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating NES MIDI example files"
    DEPENDS generate_nes_examples
)