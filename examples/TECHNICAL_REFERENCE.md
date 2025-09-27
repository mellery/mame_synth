# NES Synthesizer Technical Reference

Comprehensive technical documentation for the NES MIDI examples, including MIDI implementation details, NES hardware specifications, and development information.

## MIDI Implementation

### Channel Mapping

| MIDI Channel | NES Channel | Voice Type | Polyphony | Volume Control |
|-------------|-------------|------------|-----------|----------------|
| 0 | Pulse 1 | Square Wave | Monophonic | Full (0-127) |
| 1 | Pulse 2 | Square Wave | Monophonic | Full (0-127) |
| 2 | Triangle | Triangle Wave | Monophonic | Fixed |
| 3 | DMC | Sample-based | Monophonic | Full (0-127) |
| 9 | Noise | Noise | Monophonic | Full (0-127) |

### MIDI Message Support

#### Note Events
```
Note On:  90 nn vv (Channel 0, Note nn, Velocity vv)
Note Off: 80 nn vv (Channel 0, Note nn, Velocity vv)
```

#### Control Changes
```
CC 7:  Volume (0-127)
CC 10: Pan Position (0=left, 64=center, 127=right)
CC 64: Sustain Pedal (0-63=off, 64-127=on)
```

#### Channel Pressure
```
D0 vv: Channel Aftertouch (affects volume/timbre)
```

#### Pitch Bend
```
E0 ll hh: Pitch Bend (14-bit value, 8192=center)
```

### Note Mapping and Frequency Tables

#### Standard MIDI Note Numbers to NES Frequencies

| MIDI Note | Note Name | Frequency (Hz) | NES Period | Best Channels |
|-----------|-----------|----------------|------------|---------------|
| 24 | C1 | 32.7 | 1023 | Triangle, DMC |
| 36 | C2 | 65.4 | 511 | Triangle, DMC |
| 48 | C3 | 130.8 | 255 | Triangle, Pulse |
| 60 | C4 | 261.6 | 127 | Pulse, Triangle |
| 72 | C5 | 523.3 | 63 | Pulse |
| 84 | C6 | 1046.5 | 31 | Pulse |
| 96 | C7 | 2093.0 | 15 | Pulse (high) |

#### NES Hardware Frequency Limits
- **Minimum**: ~54 Hz (Period 1023)
- **Maximum**: ~12.4 kHz (Period 8)
- **Optimal Range**: 80 Hz - 4 kHz
- **Sweet Spot**: 200 Hz - 2 kHz

### Velocity Mapping

#### Pulse Channels (0, 1)
```cpp
// Velocity maps to duty cycle and volume
if (velocity >= 96)      duty = 3;  // 75% duty cycle
else if (velocity >= 80) duty = 2;  // 50% duty cycle
else if (velocity >= 64) duty = 1;  // 25% duty cycle
else                     duty = 0;  // 12.5% duty cycle

volume = (velocity * 15) / 127;  // Map to NES volume range 0-15
```

#### Triangle Channel (2)
```cpp
// Triangle has fixed volume, velocity affects linear counter
linear_counter = (velocity > 0) ? 127 : 0;  // On/off only
```

#### Noise Channel (9)
```cpp
// MIDI drum mapping to noise periods
switch (midi_note) {
    case 36: period = 15; break;  // Kick drum (low noise)
    case 38: period = 7;  break;  // Snare drum (mid noise)
    case 42: period = 2;  break;  // Hi-hat (high noise)
    case 44: period = 1;  break;  // Pedal hi-hat
    case 46: period = 0;  break;  // Open hi-hat
}
volume = (velocity * 15) / 127;
```

#### DMC Channel (3)
```cpp
// DMC sample rate mapping
if (midi_note < 48)      rate = 0;  // Lowest sample rate
else if (midi_note < 60) rate = 1;  // Low sample rate
else if (midi_note < 72) rate = 2;  // Medium sample rate
else                     rate = 3;  // High sample rate

volume = (velocity * 127) / 127;  // Direct mapping
```

## NES Hardware Specifications

### APU (Audio Processing Unit) Overview

#### Pulse Channels (2x)
- **Waveform**: Square wave with variable duty cycle
- **Duty Cycles**: 12.5%, 25%, 50%, 75%
- **Frequency Range**: ~54 Hz to 12.4 kHz
- **Volume Range**: 16 levels (0-15)
- **Special Features**: Hardware sweep unit

#### Triangle Channel (1x)
- **Waveform**: Triangle wave (32-step linear sequence)
- **Frequency Range**: ~27 Hz to 55.9 kHz
- **Volume**: Fixed (no volume control)
- **Special Features**: Linear counter for length control

#### Noise Channel (1x)
- **Waveform**: Pseudo-random noise
- **Periods**: 16 different noise periods
- **Volume Range**: 16 levels (0-15)
- **Modes**: Long noise (32767-bit) or short noise (93-bit)

#### DMC Channel (1x)
- **Type**: Delta modulation (sample-based)
- **Sample Rates**: 4.18 kHz to 33.14 kHz
- **Bit Depth**: 7-bit delta encoding
- **DMA**: Direct memory access for sample playback

### Timing and Clock References

#### Master Clock
- **NTSC**: 1.789773 MHz (21.477272 MHz ÷ 12)
- **PAL**: 1.662607 MHz (26.601712 MHz ÷ 16)

#### Frame Sequencer
- **NTSC**: ~240 Hz (frame rate)
- **PAL**: ~200 Hz (frame rate)

#### Timer Calculations
```cpp
// Pulse/Triangle frequency calculation
frequency = CPU_CLOCK / (16 * (period + 1))

// Noise frequency calculation
frequency = CPU_CLOCK / (16 * noise_period_table[period])

// DMC sample rate calculation
sample_rate = CPU_CLOCK / dmc_rate_table[rate]
```

## Example File Technical Details

### Basic Examples Analysis

#### `pulse1_demo.mid`
```
File Format: SMF Type 0
Track Count: 1
Timing: 480 PPQ
Duration: 1920 ticks (4 beats at 120 BPM)

MIDI Events:
00 FF 03 18 "NES Pulse Channel 1 - Duty Cycles"
00 90 3C 64  ; Note On C4, velocity 100
78 80 3C 40  ; Note Off C4 (120 ticks later)
00 90 40 50  ; Note On E4, velocity 80
78 80 40 40  ; Note Off E4
...

NES Translation:
C4 @ vel 100 → Period 127, Duty 75%, Volume 11
E4 @ vel 80  → Period 101, Duty 50%, Volume 9
```

#### `triangle_demo.mid`
```
File Format: SMF Type 0
Channel Usage: MIDI Channel 2 (NES Triangle)
Note Range: C3-G3 (optimal triangle range)
Pattern: Walking bass line

Technical Notes:
- No velocity variation (triangle has fixed volume)
- 240-tick note duration (moderate bass rhythm)
- Frequency range: 130-196 Hz (bass frequencies)
```

#### `noise_demo.mid`
```
Channel Usage: MIDI Channel 9 (standard drum channel)
Drum Mapping:
- Note 36 (C2): Long noise, period 15
- Note 42 (F#2): Short noise, period 2
- Note 38 (D2): Long noise, period 7
- Note 42 (F#2): Short noise, period 2 (softer)

Velocity Mapping:
100 → Volume 15, 80 → Volume 9, 90 → Volume 10, 60 → Volume 7
```

### Advanced Examples Analysis

#### `arpeggios.mid`
```
Technique: Rapid note alternation
Timing: 80 ticks per note (very fast)
Pattern: C-E-G major triads
Channel: MIDI 0 (Pulse 1)

Performance Requirements:
- High timing precision
- Consistent velocity
- Clean note transitions
```

#### `echo_effects.mid`
```
Multi-Channel Coordination:
Channel 0: Original note (C5, velocity 100)
Channel 1: Echo 1 (C5, velocity 60, delay 240 ticks)
Channel 0: Echo 2 (C5, velocity 30, delay 480 ticks)

Timing Calculations:
240 ticks = 0.5 beats at 480 PPQ
480 ticks = 1.0 beats at 480 PPQ
Creates 2:1:0.5 volume ratio
```

### File Format Specifications

#### MIDI Header Format
```
Chunk Type: MThd (4 bytes)
Chunk Length: 0x00000006 (4 bytes)
Format Type: 0x0000 or 0x0001 (2 bytes)
Track Count: 0x0001-0x0005 (2 bytes)
Time Division: 0x01E0 (480 PPQ) (2 bytes)
```

#### Track Header Format
```
Chunk Type: MTrk (4 bytes)
Chunk Length: Variable (4 bytes)
Track Data: Variable length MIDI events
```

#### Variable Length Encoding
```cpp
uint32_t read_variable_length(uint8_t* data, int& offset) {
    uint32_t value = 0;
    uint8_t byte;
    do {
        byte = data[offset++];
        value = (value << 7) | (byte & 0x7F);
    } while (byte & 0x80);
    return value;
}
```

## Generator Tool Implementation

### Source Code Structure
```cpp
class MIDIGenerator {
    // Core MIDI file generation
    void write_header(std::ofstream& file);
    void write_track(std::ofstream& file, const Track& track);
    void write_variable_length(std::vector<uint8_t>& data, uint32_t value);
};

// Helper functions for MIDI events
std::vector<uint8_t> note_on(uint8_t channel, uint8_t note, uint8_t velocity);
std::vector<uint8_t> note_off(uint8_t channel, uint8_t note, uint8_t velocity);
std::vector<uint8_t> control_change(uint8_t channel, uint8_t controller, uint8_t value);
```

### Build Integration
```cmake
# CMakeLists.txt entry
add_executable(generate_nes_examples generate_nes_examples.cpp)

# Custom target for regeneration
add_custom_target(generate_examples
    COMMAND generate_nes_examples
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
```

### Compilation and Execution
```bash
# Build generator
make generate_nes_examples

# Run generator
./build/tools/generate_nes_examples

# Verify output
ls examples/midi/*/*.mid
```

## Performance Optimization

### CPU Usage Considerations
- **Real-time playback**: Requires consistent timing
- **Buffer management**: Minimize audio dropouts
- **Memory allocation**: Pre-allocate MIDI event structures

### Latency Optimization
```cpp
// Recommended buffer sizes
const uint32_t OPTIMAL_BUFFER_SIZE = 512;   // samples
const uint32_t OPTIMAL_SAMPLE_RATE = 44100; // Hz
const double TARGET_LATENCY = 11.6; // milliseconds
```

### Memory Usage
```cpp
// Typical memory usage per example
struct Example {
    std::vector<MIDIEvent> events;     // ~1-5 KB per file
    std::string metadata;              // ~100-500 bytes
    uint32_t duration_ticks;           // 4 bytes
};
```

## Quality Assurance

### Validation Checks
1. **MIDI Format Compliance**: Standard MIDI File validation
2. **Timing Accuracy**: Verify tick calculations
3. **Channel Mapping**: Ensure proper NES channel assignment
4. **Frequency Range**: Check NES hardware limits
5. **File Integrity**: Binary file verification

### Testing Procedures
```bash
# Validate MIDI files
./mame_synth validate examples/midi/basic/*.mid

# Test playback
./mame_synth examples/midi/educational/all_channels.mid

# Performance analysis
./mame_synth --stats examples/midi/compositions/chiptune_style.mid
```

### Error Handling
```cpp
// Common error conditions
enum class ValidationResult {
    SUCCESS,
    INVALID_MIDI_FORMAT,
    UNSUPPORTED_CHANNEL,
    FREQUENCY_OUT_OF_RANGE,
    TIMING_ERROR,
    FILE_CORRUPTION
};
```

## Future Enhancements

### Planned Features
1. **Extended MIDI Support**: SysEx messages for NES-specific parameters
2. **Dynamic Generation**: Runtime example creation
3. **Interactive Tutorials**: Step-by-step learning examples
4. **Performance Benchmarks**: Timing and resource usage examples

### API Extensions
```cpp
// Proposed extensions
class AdvancedMIDIGenerator {
    void add_sweep_effect(uint8_t channel, sweep_config config);
    void add_vibrato(uint8_t channel, lfo_config config);
    void add_envelope(uint8_t channel, envelope_config config);
};
```

### Tool Improvements
```cpp
// Command-line interface
./generate_nes_examples --style chiptune --channels 0,1,2
./generate_nes_examples --tempo 140 --key "C minor"
./generate_nes_examples --technique arpeggios --complexity advanced
```

## Appendices

### A. Complete MIDI Event Reference
[Detailed MIDI specification tables]

### B. NES APU Register Documentation
[Hardware register mappings and bit fields]

### C. Frequency Tables
[Complete frequency-to-period conversion tables]

### D. Example File Checksums
[MD5 hashes for file verification]

This technical reference provides the foundation for understanding, modifying, and extending the NES MIDI example system. It serves developers, researchers, and advanced users who need detailed implementation information.