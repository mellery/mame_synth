# MAME Synthesizer Project Roadmap

A git project that uses MAME as a submodule to play MIDI/MusicXML files through various synths and audio devices, including NES and SNES audio.

## Phase 1: Foundation & Research ✅ COMPLETE
1. **Research MAME audio system architecture and API** ✅ - Understand how MAME handles audio devices, sound cores, and driver interfaces
2. **Set up git repository with MAME submodule** ✅ - Initialize project structure with MAME as a submodule

## Phase 2: Core Architecture
3. **Set up build system and MAME integration** ✅ - Configure build to link against MAME libraries and handle dependencies
4. **Create minimal machine context stub** - Build lightweight machine/device context for MAME audio devices
5. **Design MIDI/MusicXML parser interface** - Create abstraction for reading and parsing music files
6. **Create audio device abstraction layer** - Build interface between sequencer and MAME's device_sound_interface
7. **Design note-to-register mapping system** - Create translation layer from MIDI notes to device register writes
8. **Set up unit testing framework** - Configure testing infrastructure for the project
9. **Address licensing requirements** - Set up GPL compliance for NES APU usage

## Phase 3: Audio Device Implementation
10. **Write tests for MIDI/MusicXML parsers** - Unit tests for file parsing and data validation
11. **Implement MAME device initialization system** - Create device factory and sound_manager integration
11a. **Add MAME utility library dependencies** - Link required MAME utility and core libraries for device operation
12. **Implement NES APU wrapper class** - Wrap nesapu_device with simplified interface for external use
13. **Create NES APU note mapping** - Map MIDI notes to NES APU register writes (squares, triangle, noise)
14. **Write tests for NES APU driver** - Unit tests for device initialization and audio output
15. **Implement SNES S-DSP wrapper class** - Wrap s_dsp_device with external interface
16. **Create SNES S-DSP note mapping** - Map MIDI notes to S-DSP sample loading and playback
17. **Write tests for SNES S-DSP driver** - Unit tests for SNES audio functionality
18. **Add support for other MAME synth devices** - Extend to FM synths (YM2612, OPL3), PSG chips, etc.
19. **Write tests for additional synth devices** - Unit tests for extended audio device support

## Phase 4: Playback Engine
20. **Create MAME sound_stream integration** - Interface with MAME's stream-based audio system
21. **Implement multi-device stream mixing** - Coordinate multiple audio devices through sound_manager
22. **Create sequencer engine for playback** - Handle timing, note scheduling, and device coordination
23. **Write tests for sequencer engine** - Unit tests for timing accuracy and note scheduling
24. **Add real-time audio output system** - Interface with system audio APIs for stream playback
25. **Write tests for audio output system** - Unit tests for audio streaming and latency

## Phase 5: User Interface
26. **Build CLI interface for file input and device selection** - Command-line tool for easy usage
27. **Add device-specific configuration options** - Allow tuning of device parameters and sound characteristics
28. **Write integration tests** - End-to-end tests for complete workflow from file input to audio output
29. **Create example MIDI files** - Provide test files showcasing different device capabilities

This approach leverages MAME's extensive collection of accurately emulated audio chips while providing a modern interface for music file playback. The modular design allows incremental implementation starting with basic NES/SNES support and expanding to other devices.

## Key Architectural Decisions
- **Minimal MAME Integration**: Use only audio devices without full machine emulation
- **Stream-Based Architecture**: Leverage MAME's sound_stream system for real-time audio
- **Device Wrapper Pattern**: Abstract MAME device complexity behind simplified interfaces
- **Register Translation Layer**: Convert MIDI events to device-specific register operations
- **Modular Licensing**: Handle mixed GPL/LGPL/BSD licensing requirements appropriately