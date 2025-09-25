# MAME Synthesizer

A project that uses MAME as a submodule to play MIDI/MusicXML files through various retro audio devices including NES and SNES audio chips.

## Build Instructions

### Prerequisites
- CMake 3.16 or later
- C++17 compatible compiler (GCC, Clang)
- Git

### Building
```bash
# Clone with submodules
git clone --recurse-submodules <repository-url>
cd mame_synth

# Build
cmake -B build
cmake --build build

# Run
./build/mame_synth
```

## Project Structure
```
mame_synth/
├── mame/                   # MAME submodule
├── src/                    # Project source code
├── build/                  # Build output (generated)
├── CMakeLists.txt         # Build configuration
├── roadmap.md             # Development roadmap
└── README.md              # This file
```

## Development Status
- ✅ Phase 1: Foundation & Research - COMPLETE
- 🔄 Phase 2: Core Architecture - IN PROGRESS
  - ✅ Build system setup
  - ⏳ Minimal machine context stub
  - ⏳ MIDI/MusicXML parser interface
  - ⏳ Audio device abstraction layer
  - ⏳ Note-to-register mapping system
  - ⏳ Unit testing framework
  - ⏳ GPL licensing compliance

## License
Mixed licensing due to MAME integration:
- Project code: MIT/BSD-3-Clause
- MAME core: BSD-3-Clause
- NES APU: GPL-2.0+
- SNES S-DSP: LGPL-2.1+