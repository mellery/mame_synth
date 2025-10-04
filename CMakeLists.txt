cmake_minimum_required(VERSION 3.16)
project(mame_synth VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable debug symbols
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

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
    ${MAME_SRC_DIR}/frontend/mame
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
    -DNDEBUG  # Disable assertions for MAME code
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
    # Removed: src/mame_core/minimal_nesapu.cpp (replaced with real MAME device below)
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
    src/debug_config.cpp
    src/mame_core/debug_stub.cpp
    src/mame_core/emulator_stub.cpp
    src/mame_core/softfloat_wrapper.cpp
    src/mame_core/minimal_osd.cpp
    src/mame_core/sound_capture.cpp
    src/mame_core/mame_midi_player.cpp
    src/mame_core/osd_stubs.cpp
    src/mame_core/nanosvg_impl.cpp
    ${MAME_DIR}/build/generated/emu/layout/noscreens.cpp
)

# MAME Core files - required for device infrastructure
set(MAME_CORE_SOURCES
    ${MAME_DIR}/src/emu/drivers/empty.cpp
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
    ${MAME_DIR}/src/emu/xtal.cpp
    ${MAME_DIR}/src/emu/profiler.cpp
    # Video subsystem - needed by machine.cpp even for audio-only
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
    # OSD layer - osd_common_t implementation
    ${MAME_DIR}/src/osd/modules/lib/osdobj_common.cpp
    ${MAME_DIR}/src/osd/modules/osdmodule.cpp
    ${MAME_DIR}/src/osd/watchdog.cpp
    ${MAME_DIR}/src/osd/modules/lib/osdlib_unix.cpp
    ${MAME_DIR}/src/osd/modules/osdwindow.cpp
    ${MAME_DIR}/src/osd/modules/sound/sound_module.cpp
    # OSD module stubs - "none" implementations for modules we don't use
    ${MAME_DIR}/src/osd/modules/debugger/none.cpp
    ${MAME_DIR}/src/osd/modules/netdev/none.cpp
    ${MAME_DIR}/src/osd/modules/midi/none.cpp
    ${MAME_DIR}/src/osd/modules/render/drawnone.cpp
    ${MAME_DIR}/src/osd/modules/font/font_none.cpp
    # Input modules (common + none)
    ${MAME_DIR}/src/osd/modules/input/input_common.cpp
    ${MAME_DIR}/src/osd/modules/input/input_none.cpp
    # Monitor modules (just common - no actual monitors needed)
    ${MAME_DIR}/src/osd/modules/monitor/monitor_common.cpp
    # Output modules (none + console + network for registration)
    ${MAME_DIR}/src/osd/modules/output/none.cpp
    ${MAME_DIR}/src/osd/modules/output/console.cpp
    ${MAME_DIR}/src/osd/modules/output/network.cpp
    # Sound modules (just none - we handle audio ourselves)
    ${MAME_DIR}/src/osd/modules/sound/none.cpp
    ${MAME_DIR}/src/emu/romload.cpp
    # Debug subsystem - complete set of files
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
    ${MAME_DIR}/src/emu/ioport.cpp
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
    ${MAME_DIR}/src/osd/interface/inputseq.cpp
    ${MAME_DIR}/src/osd/strconv.cpp
    ${MAME_DIR}/src/osd/modules/file/posixptty.cpp
    ${MAME_DIR}/src/osd/modules/file/posixsocket.cpp
    ${MAME_DIR}/src/osd/modules/file/posixfile.cpp
    ${MAME_DIR}/src/osd/modules/file/posixdir.cpp
    # Memory system - needed for memory_view and handlers
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
    # Software/ROM loading system
    ${MAME_DIR}/src/emu/softlist.cpp
    ${MAME_DIR}/src/emu/softlist_dev.cpp
    ${MAME_DIR}/src/emu/image.cpp
    ${MAME_DIR}/src/emu/romentry.cpp
    # Audio effects
    ${MAME_DIR}/src/emu/audio_effects/compressor.cpp
    ${MAME_DIR}/src/emu/audio_effects/eq.cpp
    ${MAME_DIR}/src/emu/audio_effects/filter.cpp
    ${MAME_DIR}/src/emu/audio_effects/reverb.cpp
    # Debugger system
    ${MAME_DIR}/src/emu/debugger.cpp
    ${MAME_DIR}/src/emu/debug/debughlp.cpp
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
    # Network/HTTP (even if stubbed, need actual classes)
    ${MAME_DIR}/src/emu/network.cpp
    ${MAME_DIR}/src/emu/http.cpp
    # Video subsystem support
    ${MAME_DIR}/src/emu/video/rgbgen.cpp
    ${MAME_DIR}/src/emu/video/rgbsse.cpp
    ${MAME_DIR}/src/emu/recording.cpp
    # Additional utilities
    ${MAME_DIR}/src/lib/util/zippath.cpp
    # ${MAME_DIR}/src/osd/modules/lib/osdobj_common.cpp - needs osd_module_manager
    # SoftFloat for floating point emulation
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

# Audio synth driver sources
set(AUDIO_SYNTH_SOURCES
    src/mame_core/audio_synth_driver.cpp
)

# FLAC library sources (exclude OGG-related files)
file(GLOB FLAC_SOURCES ${MAME_DIR}/3rdparty/flac/src/libFLAC/*.c)
list(FILTER FLAC_SOURCES EXCLUDE REGEX ".*ogg.*")

# Add compiler flags specifically for FLAC files
# Define FLAC__U64L macro using add_compile_options for these specific files
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
    # NES APU (audio device)
    ${MAME_DIR}/src/devices/sound/nes_apu.cpp
    # M6502 CPU core (required for RP2A03)
    ${MAME_DIR}/src/devices/cpu/m6502/m6502.cpp
    ${MAME_DIR}/src/devices/cpu/m6502/m6502d.cpp
    # RP2A03 CPU (NES CPU with integrated APU)
    ${MAME_DIR}/src/devices/cpu/m6502/rp2a03.cpp
    ${MAME_DIR}/src/devices/cpu/m6502/rp2a03d.cpp
)

# Combine all MAME sources
set(MAME_SOURCES ${MAME_CORE_SOURCES} ${MAME_DEVICE_SOURCES} ${MAME_3RDPARTY_SOURCES} ${AUDIO_SYNTH_SOURCES})

# Create MAME static library (so it doesn't get recompiled on every clean build)
add_library(mame_core STATIC ${MAME_SOURCES})
target_link_libraries(mame_core z jpeg png pthread)
target_include_directories(mame_core PRIVATE
    ${MAME_DIR}/src
    ${MAME_DIR}/src/emu
    ${MAME_DIR}/src/devices
    ${MAME_DIR}/src/devices/cpu
)

# Create main executable (only PROJECT_SOURCES - our 30 files)
add_executable(mame_synth ${PROJECT_SOURCES})

# Link MAME library and other dependencies
target_link_libraries(mame_synth mame_core pthread)

# Add include directories for main executable
target_include_directories(mame_synth PRIVATE
    ${MAME_DIR}/src
    ${MAME_DIR}/src/emu
    ${MAME_DIR}/src/devices
    ${MAME_DIR}/src/devices/cpu
)

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

# Find and link SDL2 (required by osdlib_unix.cpp)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 sdl2)
if(SDL2_FOUND)
    target_include_directories(mame_synth PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(mame_synth ${SDL2_LIBRARIES})
    message(STATUS "Found SDL2: ${SDL2_LIBRARIES}")
else()
    message(FATAL_ERROR "SDL2 not found - required for OSD layer. Install with: sudo apt-get install libsdl2-dev")
endif()

# Find and link audio libraries
if(UNIX AND NOT APPLE)
    # Linux - use ALSA
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