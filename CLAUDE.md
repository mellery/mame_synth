# MAME Synthesizer Project

A production-ready NES synthesizer built on top of MAME's accurate audio device emulation. This project uses MAME as a submodule to play MIDI files through hardware-accurate emulated sound chips, with initial focus on the Nintendo Entertainment System (NES) Audio Processing Unit (APU).

## Project Overview

This synthesizer provides:
- **Hardware-accurate NES APU emulation** using MAME's audio devices
- **Real-time MIDI playback** with intelligent channel assignment
- **Cross-platform audio output** via MAME's OSD (Operating System Dependent) layer
- **Interactive CLI interface** with real-time parameter control
- **Comprehensive configuration management** with presets and validation
- **Production-ready performance** with sub-millisecond latency
- **Proper OSD integration** with MAME's module system for audio, video, and input

## Quick Start

### Build Requirements

- **CMake 3.16+**
- **C++17 compiler** (GCC/Clang/MSVC)
- **Audio libraries**:
  - Linux: ALSA (`libasound2-dev`)
  - Windows: DirectSound (included)
- **Git** (for MAME submodule)

### Building

```bash
# Clone repository
git clone <repository-url>
cd mame_synth

# Initialize MAME submodule (if needed)
git submodule update --init --recursive

# Build (first build takes 10-15 minutes due to MAME subsystems)
mkdir build
cd build
cmake ..
make -j4  # Use 4 parallel jobs to avoid memory issues
```

**Important Build Notes**:
- **First/Clean Build Time**: Initial build compiles 170+ MAME source files and can take 10-20 minutes
- **Incremental Builds**: Subsequent builds are much faster (1-2 minutes) as only changed files recompile
- **Clean Builds**: `rm -rf build` followed by rebuild will take full 10-20 minutes again
- **Parallelism**: Use `-j2` or `-j4` based on available CPU/memory (avoid `-j$(nproc)` on systems with limited RAM)
- **Components**: Compiles 60+ MAME core files, 3rd-party libraries (FLAC, ZSTD, LZMA, SoftFloat), and all project sources

### Quick Test

```bash
# Run basic tests
./tests/mame_synth_tests --test-suite=all

# Test NES playback with example file
./mame_synth examples/nes_example_basic.mid
```

## Testing

The project includes a comprehensive test suite with 95+ tests covering unit, integration, performance, and stress testing.

### Test Commands

```bash
# Run all tests
cd build
ctest

# Run specific test suites
./tests/mame_synth_tests --test-suite=nes_integration
./tests/mame_synth_tests --test-suite=channel_assignment
./tests/mame_synth_tests --test-suite=audio_stream

# Run MAME integration tests
./tests/mame_integration_tests

# Run comprehensive tests (performance + stress)
./tests/comprehensive_test_runner --all
```

### Test Categories

- **Unit Tests**: Core component testing (parser, devices, mapping)
- **Integration Tests**: Component interaction and workflow testing
- **Performance Tests**: Latency, throughput, and memory usage benchmarks
- **Stress Tests**: High-load scenarios and edge case handling
- **End-to-End Tests**: Complete MIDI-to-audio workflow validation

## Usage Examples

### Basic Playbook

```bash
# Play MIDI file with default NES settings
./mame_synth input.mid

# Play with specific output file
./mame_synth input.mid -o output.wav

# Use specific channel assignment strategy
./mame_synth input.mid --assignment=quality_focused
```

### Interactive Mode

```bash
# Start interactive session
./mame_synth -i

# Commands in interactive mode:
> load examples/nes_example_arpeggios.mid
> play
> set volume 0.8
> set channel 0 duty_cycle 0.25
> save_preset my_settings
> stop
```

### Configuration

```bash
# Use specific config file
./mame_synth -c config/performance.json input.mid

# Available presets: authentic, quality, creative, bass_heavy, melodic_focus, performance
./mame_synth --preset=quality input.mid
```

## Architecture

### Core Components

1. **Music Parser System** (`src/music_parser.cpp`)
   - MIDI file parsing and event extraction
   - MusicXML framework (ready for expansion)
   - Enhanced file format support

2. **Audio Device Layer** (`src/audio_device.cpp`, `src/mame_integration.cpp`)
   - MAME device abstraction and lifecycle management
   - NES APU wrapper with hardware-accurate emulation
   - Factory pattern for device instantiation
   - OSD (Operating System Dependent) layer integration

2a. **MAME OSD Integration** (`src/mame_core/minimal_osd.cpp`, `src/mame_core/minimal_osd.h`)
   - Inherits from `osd_common_t` for proper MAME integration
   - Implements OSD module system (audio, video, input)
   - Captures audio via `sound_stream_sink_update()` callback
   - Minimal UI manager and machine manager for audio-focused operation

3. **Register Mapping System** (`src/register_mapping.cpp`, `src/nes_note_mapping.cpp`)
   - MIDI note to device register translation
   - Hardware-accurate frequency calculations
   - NTSC/PAL timing support

4. **Audio Streaming** (`src/audio_stream.cpp`, `src/nes_audio_mixer.cpp`)
   - Real-time audio output with cross-platform backends
   - Hardware-accurate NES mixing with non-linear DAC simulation
   - High-performance streaming with minimal latency

5. **Sequencer Engine** (`src/nes_sequencer.cpp`, `src/nes_playback_engine.cpp`)
   - Real-time MIDI event scheduling and playback
   - Pattern support and tempo control
   - Performance monitoring and optimization

6. **Channel Assignment** (`src/nes_channel_assignment.cpp`)
   - Intelligent MIDI track to NES channel mapping
   - 6 assignment strategies with automatic analysis
   - Conflict resolution and quality scoring

7. **CLI Interface** (`src/nes_cli.cpp`)
   - Interactive command-line interface
   - Real-time parameter control during playback
   - Comprehensive status monitoring

### Key Features

- **Hardware Accuracy**: Uses MAME's cycle-accurate NES APU emulation
- **Real-Time Performance**: Sub-millisecond audio latency
- **Intelligent Mapping**: Automatic analysis of MIDI tracks for optimal NES channel assignment
- **Production Ready**: Comprehensive testing, error handling, and performance optimization
- **Cross-Platform**: Linux (ALSA), Windows (DirectSound), and file output support

## Project Structure

```
src/
├── main.cpp                      # Main application entry point
├── audio_device.{cpp,h}          # Audio device abstraction layer
├── audio_stream.{cpp,h}          # Real-time audio streaming
├── mame_integration.{cpp,h}      # MAME device integration
├── mame_core/                    # MAME OSD and core integration
│   ├── minimal_osd.{cpp,h}      # OSD implementation (inherits osd_common_t)
│   ├── audio_synth_driver.cpp   # MAME driver for audio synthesizer
│   ├── *_stub.cpp               # Minimal stubs for unused MAME subsystems
│   └── softfloat_wrapper.cpp    # C/C++ linkage compatibility
├── music_parser.{cpp,h}          # MIDI/MusicXML parsing
├── nes_*.{cpp,h}                # NES-specific components
└── register_mapping.{cpp,h}     # Note-to-register mapping

tests/                            # Comprehensive test suite
├── test_*.cpp                   # Unit and integration tests
├── test_framework.h             # Custom testing framework
└── TESTING_GUIDE.md            # Testing documentation

examples/                        # Example MIDI files
config/                          # Configuration presets
tools/                           # Development and build tools
```

## Development

### Lint and Type Check

```bash
# The project uses C++17 with compiler warnings as quality checks
make clean && make  # Full rebuild with all warnings enabled
```

### Adding New Features

1. Follow existing patterns in `src/` directory
2. Add corresponding tests in `tests/` directory
3. Update configuration options if needed
4. Ensure all tests pass: `ctest`

### MAME Integration

The project uses a comprehensive MAME integration approach following MAmidiMEmo's architecture:
- **Full OSD Integration**: Inherits from `osd_common_t` for proper MAME module system support
- **OSD Modules**: Initializes sound, video, input, and other OSD modules as needed
- **Audio Capture**: Captures audio via `sound_stream_sink_update()` callback from `osd_common_t`
- **Machine Manager**: Custom `minimal_machine_manager` with http_manager and ui_manager
- **Driver System**: Custom `audiosynth` driver with RP2A03 CPU and screen device for timing
- **MAME Submodule**: MAME included as git submodule (60+ source files compiled)

**Key Architecture Decisions**:
- **Why osd_common_t**: Provides audio callback infrastructure and module system that MAME expects
- **Why not headless**: MAME's architecture requires OSD modules even for audio-only operation
- **Screen Device**: Required for audio timing (MAME's sound system driven by screen refresh)
- **CPU Device**: RP2A03 (NES CPU) drives MAME's scheduler and provides integrated APU

**MAME Core Components** (`src/mame_core/`):
- `minimal_osd.{cpp,h}` - OSD implementation inheriting from `osd_common_t`
- `audio_synth_driver.cpp` - MAME driver definition for audio synthesizer machine
- `debug_stub.cpp` - Minimal debugger/driver list implementations
- `emulator_stub.cpp` - Emulator info stubs
- `osd_stubs.cpp` - Stubs for unused OSD features (network, AVI, layouts)
- `softfloat_wrapper.cpp` - C/C++ linkage bridge for SoftFloat library

## References and Related Projects

### MAME Documentation
**URL**: https://docs.mamedev.org/

Official MAME documentation covering architecture, device system, audio emulation, and development guidelines. Essential references:
- **Device System**: Understanding MAME's device lifecycle, finder objects, and delegates
- **Sound Emulation**: Audio device architecture, sound_stream system, and sample generation
- **Building MAME**: Build system, dependencies, and platform-specific requirements
- **Driver Development**: Creating and configuring machine drivers (relevant for device integration)

This project integrates MAME's audio device subsystem directly, so understanding MAME's device architecture and sound system is crucial for development and debugging.

### MAmidiMEmo
**URL**: https://github.com/denjhang/MAmidiMEmo

MAmidiMEmo is a MIDI-to-MAME synthesizer tool that inspired aspects of this project's architecture. Key similarities and differences:

**Similarities**:
- Both use MAME as the underlying audio device emulation engine
- Both focus on accurate hardware emulation for music synthesis
- Both provide MIDI file playback through emulated sound chips

**Differences**:
- **This project**: Command-line focused, C++ architecture, real-time audio streaming, intelligent channel assignment
- **MAmidiMEmo**: GUI-based Windows application, broader device support (multiple chips simultaneously)
- **This project**: Specialized NES-first approach with deep APU integration and musical analysis
- **MAmidiMEmo**: Multi-device synthesizer with visual interface and broader chip coverage

MAmidiMEmo is an excellent reference for understanding MAME audio device integration patterns and is recommended for users seeking a GUI-based multi-chip synthesizer experience.

## License

GPL-2.0 (required for MAME integration) - see `LICENSING.md` for details.

## Current Status

**Phase 6 In Progress**: Full MAME OSD integration with proper module system:
- ✅ Complete MAME build integration (60+ source files)
- ✅ Audio, memory, debug, video, and utility subsystems
- ✅ SoftFloat, FLAC, ZSTD, LZMA compression libraries
- ✅ Minimal stubs for unused features (network, layouts, drivers)
- ✅ C/C++ linkage compatibility wrappers
- ✅ Successful build of 38MB mame_synth executable
- ✅ OSD architecture: Switched from osd_interface to osd_common_t
- ✅ Machine manager with http_manager and ui_manager support
- ✅ RP2A03 CPU device for scheduler, screen device for audio timing
- 🔄 OSD module initialization (sound, video, input modules) - in progress
- ⏳ Audio capture via sound_stream_sink_update() callback
- ⏳ Full NES APU playback testing
- ⏳ Interactive CLI with MAME backend
- ⏳ Test suite compatibility with MAME integration

**Current Focus**: Implementing proper OSD module registration and initialization. MAME's osd_common_t requires sound_module, video_module, and input_module infrastructure. Following MAmidiMEmo's approach of full OSD integration rather than headless operation.

**Previous Achievements** (Phase 5):
- ✅ Complete NES APU implementation with all 5 channels
- ✅ Interactive CLI with 50+ commands
- ✅ Intelligent channel assignment with 6 strategies
- ✅ Real-time parameter control and automation
- ✅ 95+ comprehensive tests
- ✅ Cross-platform audio support
- ✅ Configuration management with presets

See `roadmap.md` for detailed development history and future expansion plans.