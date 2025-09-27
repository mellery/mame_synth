# NES Synthesizer Integration Tests

This directory contains comprehensive integration tests for the NES Synthesizer project, covering end-to-end workflows from MIDI input through audio output.

## Test Suite Overview

### Core Integration Tests

#### `test_nes_integration.cpp`
**Complete NES workflow testing** - Tests the full pipeline from MIDI file parsing through NES playback to audio output.

**Tests included:**
- `complete_midi_to_audio_workflow` - Full MIDI → Audio pipeline
- `configuration_system_workflow` - Configuration save/load/preset functionality
- `realtime_control_workflow` - Real-time parameter control integration
- `cli_command_workflow` - CLI command execution and integration
- `multi_channel_playback` - All 5 NES channels (2 pulse, triangle, noise, DMC)
- `error_handling_workflow` - Error recovery and robustness testing
- `performance_monitoring` - Performance statistics and monitoring
- `configuration_integration_with_engine` - Config → Engine integration

#### `test_cli_integration.cpp`
**Command-line interface testing** - Comprehensive CLI functionality testing.

**Tests included:**
- `basic_command_execution` - Help, version, invalid command handling
- `configuration_commands` - config-load, config-save, config-show, presets
- `realtime_control_commands` - cv, cp, cm, pd, fc, mc commands
- `midi_control_commands` - MIDI mapping and learn mode
- `parameter_control_commands` - Advanced realtime-set/get functionality
- `command_argument_parsing` - Argument validation and error handling
- `error_recovery_and_state` - CLI state management after errors
- `configuration_persistence` - Setting persistence across operations

#### `test_workflow_integration.cpp`
**Cross-component workflow testing** - Tests complex interactions between multiple components.

**Tests included:**
- `config_to_playback_workflow` - Configuration → Engine → Playback chain
- `cli_to_realtime_control_workflow` - CLI → Real-time parameter control
- `configuration_change_during_playback` - Live parameter changes
- `multi_component_stress_test` - High-load scenario with all components
- `persistent_configuration_workflow` - Configuration persistence across workflows

## Test Architecture

### Framework Features
- **Custom test framework** (`test_framework.h`) - No external dependencies
- **Assertion macros** - ASSERT_TRUE, ASSERT_EQ, ASSERT_NEAR, etc.
- **Test registration** - REGISTER_TEST macro for automatic discovery
- **Suite organization** - Tests grouped by functionality
- **Performance timing** - Automatic test duration measurement

### Test Data Management
- **Synthetic MIDI files** - Generated programmatically for consistent testing
- **Configuration files** - JSON test configurations for various scenarios
- **Audio output** - File-based audio output for verification
- **Temporary files** - Automatic cleanup of test artifacts

### Integration Points Tested

#### 1. MIDI Input Pipeline
- MIDI file parsing and validation
- Music data structure population
- Error handling for invalid files

#### 2. NES Audio Engine
- Engine initialization and configuration
- Music loading and playback control
- Multi-channel audio processing
- Hardware-accurate NES emulation

#### 3. Configuration System
- JSON configuration file handling
- Preset management (performance, quality, authentic, creative)
- Runtime configuration changes
- Configuration validation

#### 4. Real-time Control
- Parameter registration and management
- MIDI controller mapping
- Parameter automation
- Preset save/load functionality

#### 5. CLI Interface
- Command parsing and execution
- Error handling and recovery
- Help and documentation system
- Interactive command processing

## Running Tests

### Individual Test Suites
```bash
# Run specific test suite
./mame_synth_tests --test-suite=nes_integration
./mame_synth_tests --test-suite=cli_integration
./mame_synth_tests --test-suite=workflow_integration

# Run all integration tests
./mame_synth_tests --test-suite=all
```

### CMake Test Integration
```bash
# Run via CMake test runner
make test

# Or run specific named tests
ctest -R nes_integration_tests
ctest -R cli_integration_tests
ctest -R workflow_integration_tests
```

## Test Coverage

### Functional Coverage
- ✅ **MIDI File Processing** - Parser, validator, error handling
- ✅ **NES Audio Engine** - All 5 channels, mixing, streaming
- ✅ **Configuration Management** - Load, save, presets, validation
- ✅ **Real-time Control** - Parameters, MIDI, automation, presets
- ✅ **CLI Interface** - Commands, arguments, error handling
- ✅ **Cross-component Integration** - Workflow combinations

### Scenario Coverage
- ✅ **Happy Path** - Normal operation scenarios
- ✅ **Error Conditions** - Invalid inputs, file errors, state errors
- ✅ **Edge Cases** - Boundary values, empty inputs, large inputs
- ✅ **Performance** - Load testing, timing verification
- ✅ **Persistence** - Configuration and state persistence

### Component Integration Coverage
- ✅ **Config → Engine** - Configuration affects engine behavior
- ✅ **CLI → Realtime** - CLI commands control real-time parameters
- ✅ **MIDI → Audio** - Complete audio pipeline functionality
- ✅ **Engine → Output** - Audio streaming and file output
- ✅ **Parameters → Playback** - Real-time parameter changes during playback

## Test Design Principles

### 1. **Isolation**
- Each test is independent and can run in any order
- Tests clean up their own temporary files
- No shared state between tests

### 2. **Determinism**
- Synthetic test data ensures consistent results
- File-based audio output for verification
- Controlled timing for real-time tests

### 3. **Comprehensiveness**
- Tests cover normal operation and error conditions
- Integration tests complement unit tests
- End-to-end workflow validation

### 4. **Performance Awareness**
- Tests use minimal sample rates and buffer sizes
- Short audio clips for faster execution
- Performance monitoring validation

### 5. **Maintainability**
- Helper functions reduce code duplication
- Clear test names and documentation
- Modular test organization

## Future Enhancements

### Additional Test Scenarios
- **Multi-file batch processing** - Testing directory operations
- **Long-running stability** - Extended playback testing
- **Memory pressure** - Large file and high-polyphony testing
- **Platform-specific** - Audio backend specific testing
- **Concurrency** - Multi-threaded operation testing

### Test Infrastructure
- **Audio output validation** - Waveform analysis and verification
- **Performance benchmarking** - Automated performance regression detection
- **Test data generation** - More sophisticated MIDI test file creation
- **Coverage analysis** - Code coverage measurement integration

## Dependencies

### Required Components
- NES Playback Engine (`nes_playback_engine.h/.cpp`)
- CLI Interface (`nes_cli.h/.cpp`)
- Configuration System (`nes_config.h/.cpp`)
- Real-time Control (`nes_realtime_control.h/.cpp`)
- Music Parser (`music_parser.h/.cpp`)

### External Dependencies
- **Standard Library** - File I/O, threading, timing
- **Audio Backend** - ALSA/DirectSound (optional - tests use file output)
- **CMake** - Build system integration

The integration test suite provides comprehensive validation of the complete NES synthesizer system, ensuring robustness and reliability for end-users.