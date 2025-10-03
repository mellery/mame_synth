cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Find required packages for testing
find_package(PkgConfig REQUIRED)

# Use a simple testing framework (no external dependencies)
# We'll create our own minimal testing framework

# MAME directories
set(MAME_DIR ${CMAKE_SOURCE_DIR}/mame)
set(MAME_SRC_DIR ${MAME_DIR}/src)

# Include parent directories for access to source files
include_directories(
    ../src
    ${MAME_SRC_DIR}
    ${MAME_SRC_DIR}/emu
    ${MAME_SRC_DIR}/lib
    ${MAME_SRC_DIR}/lib/util
    ${MAME_SRC_DIR}/osd
    ${MAME_SRC_DIR}/devices
    ${MAME_DIR}/3rdparty
    ${MAME_DIR}/3rdparty/rapidjson/include
    ${MAME_DIR}/3rdparty/expat/lib
    ${MAME_DIR}/3rdparty/utf8proc
    ${MAME_DIR}/3rdparty/lzma/C
    ${MAME_DIR}/3rdparty/zstd/lib
    ${MAME_DIR}/3rdparty/zstd/lib/common
    ${MAME_DIR}/3rdparty/flac/include
    ${MAME_DIR}/3rdparty/flac/src/libFLAC/include
    ${MAME_DIR}/3rdparty/softfloat3/source/include
    ${MAME_DIR}/3rdparty/softfloat3/source/8086-SSE
    ${MAME_DIR}/3rdparty/softfloat3/build/Linux-386-SSE2-GCC
    ${MAME_DIR}/3rdparty/asio/include
    ${MAME_DIR}/build/generated/emu/layout
    ${MAME_DIR}/build/generated/emu/rendersw
    ${MAME_DIR}/build/generated/emu
)

# Define preprocessor flags to match MAME's build
add_definitions(
    -DLSB_FIRST=1
    -DFLAC__NO_DLL
    -DPTR64=1
    -DMAME_DEBUG=0
    -DCRLF=2
    # Expat XML library configuration
    -DXML_GE=1
    -DXML_CONTEXT_BYTES=1024
    -DHAVE_GETRANDOM
    # ZSTD configuration - disable assembly optimizations
    -DDYNAMIC_BMI2=0
    # FLAC configuration
    -DPACKAGE_VERSION="1.4.3"
    -DFLAC__HAS_OGG=0
    -DHAVE_LROUND=1
    -DFLAC__NO_DLL
    -D__STDC_FORMAT_MACROS
    # SoftFloat configuration
    -DSOFTFLOAT_FAST_INT64
    -DSOFTFLOAT_ROUND_ODD
)

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
    test_nes_playback_engine.cpp
    test_end_to_end_integration.cpp
    test_midi_wav_validation.cpp
)

# MAME Core files - import from parent CMakeLists.txt
# Note: Using CMAKE_SOURCE_DIR to reference parent directory
set(MAME_CORE_SOURCES
    # Core device system
    ${MAME_DIR}/src/emu/device.cpp
    ${MAME_DIR}/src/emu/devdelegate.cpp
    ${MAME_DIR}/src/emu/devcb.cpp
    ${MAME_DIR}/src/emu/devfind.cpp
    ${MAME_DIR}/src/emu/diexec.cpp
    ${MAME_DIR}/src/emu/diimage.cpp
    ${MAME_DIR}/src/emu/dimemory.cpp
    ${MAME_DIR}/src/emu/dinetwork.cpp
    ${MAME_DIR}/src/emu/dislot.cpp
    ${MAME_DIR}/src/emu/disound.cpp
    ${MAME_DIR}/src/emu/distate.cpp
    ${MAME_DIR}/src/emu/machine.cpp
    ${MAME_DIR}/src/emu/schedule.cpp
    ${MAME_DIR}/src/emu/sound.cpp
    ${MAME_DIR}/src/emu/main.cpp
    ${MAME_DIR}/src/emu/speaker.cpp
    ${MAME_DIR}/src/emu/resampler.cpp
    ${MAME_DIR}/src/emu/audio_effects/aeffect.cpp
    ${MAME_DIR}/src/emu/emumem.cpp
    ${MAME_DIR}/src/emu/emumem_aspace.cpp
    ${MAME_DIR}/src/emu/addrmap.cpp
    ${MAME_DIR}/src/emu/emuopts.cpp
    ${MAME_DIR}/src/emu/emucore.cpp
    ${MAME_DIR}/src/emu/attotime.cpp
    ${MAME_DIR}/src/emu/mconfig.cpp
    ${MAME_DIR}/src/emu/validity.cpp
    # Video subsystem
    ${MAME_DIR}/src/emu/video.cpp
    ${MAME_DIR}/src/emu/screen.cpp
    ${MAME_DIR}/src/emu/tilemap.cpp
    ${MAME_DIR}/src/emu/drawgfx.cpp
    ${MAME_DIR}/src/emu/dipalette.cpp
    ${MAME_DIR}/src/emu/digfx.cpp
    ${MAME_DIR}/src/emu/render.cpp
    ${MAME_DIR}/src/emu/rendutil.cpp
    ${MAME_DIR}/src/emu/rendlay.cpp
    ${MAME_DIR}/src/emu/rendfont.cpp
    ${MAME_DIR}/src/emu/emupal.cpp
    ${MAME_DIR}/src/emu/driver.cpp
    ${MAME_DIR}/src/emu/drivenum.cpp
    ${MAME_DIR}/src/emu/parameters.cpp
    ${MAME_DIR}/src/emu/romload.cpp
    # Debug subsystem
    ${MAME_DIR}/src/emu/debugger.cpp
    ${MAME_DIR}/src/emu/debug/debughlp.cpp
    ${MAME_DIR}/src/emu/debug/debugcpu.cpp
    ${MAME_DIR}/src/emu/debug/debugcon.cpp
    ${MAME_DIR}/src/emu/debug/express.cpp
    ${MAME_DIR}/src/emu/debug/debugcmd.cpp
    ${MAME_DIR}/src/emu/debug/debugvw.cpp
    ${MAME_DIR}/src/emu/debug/debugbuf.cpp
    ${MAME_DIR}/src/emu/debug/textbuf.cpp
    ${MAME_DIR}/src/emu/debug/points.cpp
    ${MAME_DIR}/src/emu/debug/dvbpoints.cpp
    ${MAME_DIR}/src/emu/debug/dvwpoints.cpp
    ${MAME_DIR}/src/emu/debug/dvstate.cpp
    ${MAME_DIR}/src/emu/debug/dvtext.cpp
    ${MAME_DIR}/src/emu/debug/dvmemory.cpp
    ${MAME_DIR}/src/emu/debug/dvdisasm.cpp
    ${MAME_DIR}/src/emu/debug/dvepoints.cpp
    ${MAME_DIR}/src/emu/debug/dvrpoints.cpp
    ${MAME_DIR}/src/emu/didisasm.cpp
    ${MAME_DIR}/src/emu/save.cpp
    ${MAME_DIR}/src/emu/output.cpp
    ${MAME_DIR}/src/emu/input.cpp
    ${MAME_DIR}/src/emu/inputdev.cpp
    ${MAME_DIR}/src/emu/natkeyboard.cpp
    ${MAME_DIR}/src/emu/config.cpp
    ${MAME_DIR}/src/emu/fileio.cpp
    ${MAME_DIR}/src/emu/bookkeeping.cpp
    ${MAME_DIR}/src/osd/osdcore.cpp
    ${MAME_DIR}/src/osd/osdsync.cpp
    ${MAME_DIR}/src/osd/strconv.cpp
    # OSD modules (requires SDL2)
    ${MAME_DIR}/src/osd/modules/lib/osdobj_common.cpp
    ${MAME_DIR}/src/osd/modules/osdmodule.cpp
    ${MAME_DIR}/src/osd/watchdog.cpp
    ${MAME_DIR}/src/osd/modules/lib/osdlib_unix.cpp
    ${MAME_DIR}/src/emu/ioport.cpp
    # Utility libraries
    ${MAME_DIR}/src/lib/util/strformat.cpp
    ${MAME_DIR}/src/lib/util/vecstream.cpp
    ${MAME_DIR}/src/lib/util/delegate.cpp
    ${MAME_DIR}/src/lib/util/mfpresolve.cpp
    ${MAME_DIR}/src/lib/util/xmlfile.cpp
    ${MAME_DIR}/src/lib/util/ioprocs.cpp
    ${MAME_DIR}/src/lib/util/options.cpp
    ${MAME_DIR}/src/lib/util/path.cpp
    ${MAME_DIR}/src/lib/util/language.cpp
    ${MAME_DIR}/src/lib/util/corefile.cpp
    ${MAME_DIR}/src/lib/util/corestr.cpp
    ${MAME_DIR}/src/lib/util/unicode.cpp
    ${MAME_DIR}/src/lib/util/ioprocsfilter.cpp
    ${MAME_DIR}/src/lib/util/unzip.cpp
    ${MAME_DIR}/src/lib/util/un7z.cpp
    ${MAME_DIR}/src/lib/util/hashing.cpp
    ${MAME_DIR}/src/lib/util/hash.cpp
    ${MAME_DIR}/src/lib/util/timeconv.cpp
    ${MAME_DIR}/src/lib/util/chdcodec.cpp
    ${MAME_DIR}/src/lib/util/chd.cpp
    ${MAME_DIR}/src/lib/util/cdrom.cpp
    ${MAME_DIR}/src/lib/util/flac.cpp
    ${MAME_DIR}/src/lib/util/huffman.cpp
    ${MAME_DIR}/src/lib/util/avhuff.cpp
    ${MAME_DIR}/src/lib/util/bitmap.cpp
    ${MAME_DIR}/src/lib/util/palette.cpp
    ${MAME_DIR}/src/lib/util/png.cpp
    ${MAME_DIR}/src/lib/util/msdib.cpp
    ${MAME_DIR}/src/lib/util/wavwrite.cpp
    ${MAME_DIR}/src/lib/util/disasmintf.cpp
    ${MAME_DIR}/src/lib/util/aviio.cpp
    ${MAME_DIR}/src/lib/util/path_to_regex.cpp
    ${MAME_DIR}/src/lib/util/zippath.cpp
    ${MAME_DIR}/src/osd/interface/inputseq.cpp
    ${MAME_DIR}/src/osd/modules/file/posixfile.cpp
    ${MAME_DIR}/src/osd/modules/file/posixdir.cpp
    ${MAME_DIR}/src/osd/modules/file/posixptty.cpp
    ${MAME_DIR}/src/osd/modules/file/posixsocket.cpp
    ${MAME_DIR}/src/osd/modules/osdwindow.cpp
    # Memory system
    ${MAME_DIR}/src/emu/emumem_mview.cpp
    ${MAME_DIR}/src/emu/emumem_hedp.cpp
    ${MAME_DIR}/src/emu/emumem_hedr0.cpp
    ${MAME_DIR}/src/emu/emumem_hedr1.cpp
    ${MAME_DIR}/src/emu/emumem_hedr2.cpp
    ${MAME_DIR}/src/emu/emumem_hedr3.cpp
    ${MAME_DIR}/src/emu/emumem_hedw0.cpp
    ${MAME_DIR}/src/emu/emumem_hedw1.cpp
    ${MAME_DIR}/src/emu/emumem_hedw2.cpp
    ${MAME_DIR}/src/emu/emumem_hedw3.cpp
    ${MAME_DIR}/src/emu/emumem_hem.cpp
    ${MAME_DIR}/src/emu/emumem_hep.cpp
    ${MAME_DIR}/src/emu/emumem_het.cpp
    ${MAME_DIR}/src/emu/emumem_heu.cpp
    ${MAME_DIR}/src/emu/emumem_heun.cpp
    ${MAME_DIR}/src/emu/emumem_hws.cpp
    ${MAME_DIR}/src/emu/emumem_mud.cpp
    # Software/ROM loading
    ${MAME_DIR}/src/emu/softlist.cpp
    ${MAME_DIR}/src/emu/softlist_dev.cpp
    ${MAME_DIR}/src/emu/image.cpp
    ${MAME_DIR}/src/emu/romentry.cpp
    # Audio effects
    ${MAME_DIR}/src/emu/audio_effects/compressor.cpp
    ${MAME_DIR}/src/emu/audio_effects/eq.cpp
    ${MAME_DIR}/src/emu/audio_effects/filter.cpp
    ${MAME_DIR}/src/emu/audio_effects/reverb.cpp
    # UI and crosshair
    ${MAME_DIR}/src/emu/uiinput.cpp
    ${MAME_DIR}/src/emu/crsshair.cpp
    # Additional device interfaces
    ${MAME_DIR}/src/emu/devcpu.cpp
    ${MAME_DIR}/src/emu/dinvram.cpp
    ${MAME_DIR}/src/emu/dipty.cpp
    ${MAME_DIR}/src/emu/dirtc.cpp
    ${MAME_DIR}/src/emu/diserial.cpp
    ${MAME_DIR}/src/emu/divideo.cpp
    ${MAME_DIR}/src/emu/divtlb.cpp
    # Empty driver stub
    ${MAME_DIR}/src/emu/drivers/empty.cpp
    # Memory array
    ${MAME_DIR}/src/emu/memarray.cpp
    # Network/HTTP
    ${MAME_DIR}/src/emu/network.cpp
    ${MAME_DIR}/src/emu/http.cpp
    # Video subsystem support
    ${MAME_DIR}/src/emu/video/rgbgen.cpp
    ${MAME_DIR}/src/emu/video/rgbsse.cpp
    ${MAME_DIR}/src/emu/recording.cpp
    # Additional utilities
    ${MAME_DIR}/src/lib/util/zippath.cpp
    # SoftFloat library
    ${MAME_DIR}/3rdparty/softfloat3/source/softfloat_state.c
    ${MAME_DIR}/3rdparty/softfloat3/source/8086-SSE/softfloat_raiseFlags.c
    ${MAME_DIR}/3rdparty/softfloat3/source/extF80M_to_f64.c
    ${MAME_DIR}/3rdparty/softfloat3/source/extF80_to_f64.c
    ${MAME_DIR}/3rdparty/softfloat3/source/s_shortShiftRightJam64.c
    ${MAME_DIR}/3rdparty/softfloat3/source/8086/s_extF80UIToCommonNaN.c
    ${MAME_DIR}/3rdparty/softfloat3/source/s_roundPackToF64.c
    ${MAME_DIR}/3rdparty/softfloat3/source/s_normRoundPackToF64.c
    ${MAME_DIR}/3rdparty/softfloat3/source/s_shiftRightJam64.c
    ${MAME_DIR}/3rdparty/softfloat3/source/8086-SSE/s_commonNaNToF64UI.c
)

# FLAC library sources (exclude OGG-related files)
file(GLOB FLAC_SOURCES ${MAME_DIR}/3rdparty/flac/src/libFLAC/*.c)
list(FILTER FLAC_SOURCES EXCLUDE REGEX ".*ogg.*")

# Add compiler flags specifically for FLAC files
foreach(flac_file ${FLAC_SOURCES})
    set_source_files_properties(${flac_file} PROPERTIES
        COMPILE_FLAGS "-include stdint.h -include inttypes.h"
    )
endforeach()

# MAME 3rdparty libraries
set(MAME_3RDPARTY_SOURCES
    ${FLAC_SOURCES}
    ${MAME_DIR}/3rdparty/expat/lib/xmlparse.c
    ${MAME_DIR}/3rdparty/expat/lib/xmlrole.c
    ${MAME_DIR}/3rdparty/expat/lib/xmltok.c
    ${MAME_DIR}/3rdparty/utf8proc/utf8proc.c
    # LZMA/7z decompression
    ${MAME_DIR}/3rdparty/lzma/C/LzmaDec.c
    ${MAME_DIR}/3rdparty/lzma/C/Lzma2Dec.c
    ${MAME_DIR}/3rdparty/lzma/C/7zAlloc.c
    ${MAME_DIR}/3rdparty/lzma/C/7zArcIn.c
    ${MAME_DIR}/3rdparty/lzma/C/7zBuf.c
    ${MAME_DIR}/3rdparty/lzma/C/7zCrc.c
    ${MAME_DIR}/3rdparty/lzma/C/7zCrcOpt.c
    ${MAME_DIR}/3rdparty/lzma/C/7zDec.c
    ${MAME_DIR}/3rdparty/lzma/C/7zStream.c
    ${MAME_DIR}/3rdparty/lzma/C/Bcj2.c
    ${MAME_DIR}/3rdparty/lzma/C/Bra.c
    ${MAME_DIR}/3rdparty/lzma/C/Bra86.c
    ${MAME_DIR}/3rdparty/lzma/C/BraIA64.c
    ${MAME_DIR}/3rdparty/lzma/C/CpuArch.c
    ${MAME_DIR}/3rdparty/lzma/C/Delta.c
    ${MAME_DIR}/3rdparty/lzma/C/Ppmd7.c
    ${MAME_DIR}/3rdparty/lzma/C/Ppmd7Dec.c
    ${MAME_DIR}/3rdparty/lzma/C/LzmaEnc.c
    ${MAME_DIR}/3rdparty/lzma/C/LzFind.c
    ${MAME_DIR}/3rdparty/lzma/C/LzFindMt.c
    ${MAME_DIR}/3rdparty/lzma/C/Threads.c
    # ZSTD decompression
    ${MAME_DIR}/3rdparty/zstd/lib/common/entropy_common.c
    ${MAME_DIR}/3rdparty/zstd/lib/common/error_private.c
    ${MAME_DIR}/3rdparty/zstd/lib/common/fse_decompress.c
    ${MAME_DIR}/3rdparty/zstd/lib/common/zstd_common.c
    ${MAME_DIR}/3rdparty/zstd/lib/common/xxhash.c
    ${MAME_DIR}/3rdparty/zstd/lib/decompress/huf_decompress.c
    ${MAME_DIR}/3rdparty/zstd/lib/decompress/zstd_ddict.c
    ${MAME_DIR}/3rdparty/zstd/lib/decompress/zstd_decompress.c
    ${MAME_DIR}/3rdparty/zstd/lib/decompress/zstd_decompress_block.c
    # ZSTD compression
    ${MAME_DIR}/3rdparty/zstd/lib/compress/fse_compress.c
    ${MAME_DIR}/3rdparty/zstd/lib/compress/hist.c
    ${MAME_DIR}/3rdparty/zstd/lib/compress/huf_compress.c
    ${MAME_DIR}/3rdparty/zstd/lib/compress/zstd_compress.c
    ${MAME_DIR}/3rdparty/zstd/lib/compress/zstd_compress_literals.c
    ${MAME_DIR}/3rdparty/zstd/lib/compress/zstd_compress_sequences.c
    ${MAME_DIR}/3rdparty/zstd/lib/compress/zstd_compress_superblock.c
    ${MAME_DIR}/3rdparty/zstd/lib/compress/zstd_double_fast.c
    ${MAME_DIR}/3rdparty/zstd/lib/compress/zstd_fast.c
    ${MAME_DIR}/3rdparty/zstd/lib/compress/zstd_lazy.c
    ${MAME_DIR}/3rdparty/zstd/lib/compress/zstd_ldm.c
    ${MAME_DIR}/3rdparty/zstd/lib/compress/zstd_opt.c
)

# MAME Audio device source files
set(MAME_DEVICE_SOURCES
    ${CMAKE_SOURCE_DIR}/mame/src/devices/sound/nes_apu.cpp
)

# Minimal emulator and OSD stubs
set(EMULATOR_STUB_SOURCE
    ../src/mame_core/emulator_stub.cpp
    ../src/mame_core/debug_stub.cpp
    # Removed mame_stubs.cpp - conflicts with real MAME OSD
    ../src/mame_core/softfloat_wrapper.cpp
    ../src/mame_core/audio_synth_driver.cpp
    ../src/mame_core/nanosvg_impl.cpp
    ${MAME_DIR}/build/generated/emu/layout/noscreens.cpp
)

set(OSD_STUB_SOURCES
    # Removed osd_stub.cpp - conflicts with real MAME OSD (osdlib_unix.cpp)
    ../src/mame_core/minimal_osd.cpp
    ../src/mame_core/osd_stubs.cpp
    # NOTE: Using real input.cpp from MAME, not input_stub.cpp
    # MAME "none" module implementations - provide FONT_NONE, RENDERER_NONE, etc.
    ${MAME_DIR}/src/osd/modules/debugger/none.cpp
    ${MAME_DIR}/src/osd/modules/netdev/none.cpp
    ${MAME_DIR}/src/osd/modules/midi/none.cpp
    ${MAME_DIR}/src/osd/modules/render/drawnone.cpp
    ${MAME_DIR}/src/osd/modules/font/font_none.cpp
    ${MAME_DIR}/src/osd/modules/input/input_common.cpp
    ${MAME_DIR}/src/osd/modules/input/input_none.cpp
    ${MAME_DIR}/src/osd/modules/monitor/monitor_common.cpp
    ${MAME_DIR}/src/osd/modules/output/none.cpp
    ${MAME_DIR}/src/osd/modules/output/console.cpp
    ${MAME_DIR}/src/osd/modules/output/network.cpp
    ${MAME_DIR}/src/osd/modules/sound/none.cpp
    ${MAME_DIR}/src/osd/modules/sound/sound_module.cpp
)

# Source files to test (exclude main.cpp)
set(LIB_SOURCES
    ../src/music_parser.cpp
    ../src/audio_device.cpp
    ../src/register_mapping.cpp
    ../src/mame_integration.cpp
    ../src/mame_core/mame_minimal.cpp
    ../src/mame_core/minimal_osd.cpp
    # Removed: ../src/mame_core/minimal_nesapu.cpp (replaced with real MAME device)
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
    ../src/debug_config.cpp
)

# Combine all MAME sources
set(MAME_SOURCES ${MAME_CORE_SOURCES} ${MAME_DEVICE_SOURCES} ${MAME_3RDPARTY_SOURCES} ${EMULATOR_STUB_SOURCE} ${OSD_STUB_SOURCES})

# Create test executable
add_executable(mame_synth_tests ${TEST_SOURCES} ${LIB_SOURCES} ${MAME_SOURCES})

# Add MAME include directories
target_include_directories(mame_synth_tests PRIVATE
    ${MAME_DIR}/src
    ${MAME_DIR}/src/emu
    ${MAME_DIR}/src/devices
    ${MAME_DIR}/src/devices/cpu
)

# Disable MAME assertions
target_compile_definitions(mame_synth_tests PRIVATE NDEBUG)

# Link compression, image, and threading libraries
target_link_libraries(mame_synth_tests z jpeg png pthread)

# Create MAME integration test executable
add_executable(mame_integration_tests
    test_mame_integration.cpp
    ../src/mame_integration.cpp
    ../src/mame_core/mame_minimal.cpp
    ../src/mame_core/minimal_osd.cpp
    # Removed: ../src/mame_core/minimal_nesapu.cpp (replaced with real MAME device)
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
    ../src/debug_config.cpp
    ../src/nes_channel_assignment.cpp
    ../src/nes_channel_assignment_cli.cpp
    ../src/nes_cli.cpp
    ${MAME_SOURCES}
)

# Add MAME include directories for mame_integration_tests
target_include_directories(mame_integration_tests PRIVATE
    ${MAME_DIR}/src
    ${MAME_DIR}/src/emu
    ${MAME_DIR}/src/devices
    ${MAME_DIR}/src/devices/cpu
)

# Link compression, image, and threading libraries for mame_integration_tests
target_link_libraries(mame_integration_tests z jpeg png pthread)

# Find and link SDL2 (required by MAME OSD layer)
pkg_check_modules(SDL2 sdl2)
if(SDL2_FOUND)
    target_include_directories(mame_synth_tests PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(mame_synth_tests ${SDL2_LIBRARIES})

    target_include_directories(mame_integration_tests PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(mame_integration_tests ${SDL2_LIBRARIES})
    message(STATUS "Found SDL2 for tests: ${SDL2_LIBRARIES}")
else()
    message(FATAL_ERROR "SDL2 not found - required for MAME OSD layer. Install with: sudo apt-get install libsdl2-dev")
endif()

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