# MAME Synthesizer Project Roadmap

A git project that uses MAME as a submodule to play MIDI/MusicXML files through various synths and audio devices, including NES and SNES audio.

## Phase 1: Foundation & Research
1. **Research MAME audio system architecture and API** - Understand how MAME handles audio devices, sound cores, and driver interfaces
2. **Set up git repository with MAME submodule** - Initialize project structure with MAME as a submodule

## Phase 2: Core Architecture
3. **Design MIDI/MusicXML parser interface** - Create abstraction for reading and parsing music files
4. **Create audio device abstraction layer** - Build interface between your sequencer and MAME's audio devices
5. **Set up unit testing framework** - Configure testing infrastructure for the project

## Phase 3: Audio Device Implementation
6. **Write tests for MIDI/MusicXML parsers** - Unit tests for file parsing and data validation
7. **Implement NES APU audio driver interface** - Interface with MAME's NES 2A03 audio processor unit
8. **Write tests for NES APU driver** - Unit tests for audio device interface and output
9. **Implement SNES SPC700 audio driver interface** - Interface with MAME's SNES sound processor
10. **Write tests for SNES SPC700 driver** - Unit tests for SNES audio functionality
11. **Add support for other MAME synth devices** - Extend to FM synths (YM2612, OPL3), PSG chips, etc.
12. **Write tests for additional synth devices** - Unit tests for extended audio device support

## Phase 4: Playback Engine
13. **Create sequencer engine for playback** - Handle timing, note scheduling, and device coordination
14. **Write tests for sequencer engine** - Unit tests for timing accuracy and note scheduling
15. **Add real-time audio output system** - Interface with system audio APIs for playback
16. **Write tests for audio output system** - Unit tests for audio streaming and latency

## Phase 5: User Interface
17. **Build CLI interface for file input and device selection** - Command-line tool for easy usage
18. **Write integration tests** - End-to-end tests for complete workflow from file input to audio output

This approach leverages MAME's extensive collection of accurately emulated audio chips while providing a modern interface for music file playback. The modular design allows incremental implementation starting with basic NES/SNES support and expanding to other devices.