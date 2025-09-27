# NES Synthesizer Example Files

This directory contains comprehensive examples showcasing the capabilities of the NES Audio Processing Unit (APU) when used as a synthesizer. These MIDI files demonstrate proper channel usage, NES-specific techniques, and musical composition strategies optimized for NES hardware.

## Directory Structure

```
examples/
├── midi/                      # MIDI example files
│   ├── basic/                 # Individual channel demonstrations
│   ├── techniques/            # NES-specific audio techniques
│   ├── compositions/          # Complete musical examples
│   └── educational/           # Learning and reference materials
├── config/                    # Example configurations (from previous tasks)
└── README.md                  # This file
```

## Example Categories

### 1. Basic Channel Demonstrations (`basic/`)

These files demonstrate each of the 5 NES audio channels individually, showing their unique characteristics and optimal usage patterns.

#### `pulse1_demo.mid` - Pulse Channel 1
- **Purpose**: Demonstrates pulse wave generation with different duty cycles
- **Channels Used**: MIDI Channel 0 → NES Pulse 1
- **Techniques**: Duty cycle variations (12.5%, 25%, 50%, 75%)
- **Musical Content**: Simple melody showcasing different timbres
- **Duration**: ~4 seconds

#### `pulse2_demo.mid` - Pulse Channel 2
- **Purpose**: Shows pulse wave harmony and secondary melodies
- **Channels Used**: MIDI Channel 1 → NES Pulse 2
- **Techniques**: Harmonic intervals, melody doubling
- **Musical Content**: Harmony line complementing pulse 1
- **Duration**: ~4 seconds

#### `triangle_demo.mid` - Triangle Channel
- **Purpose**: Demonstrates triangle wave bass lines and low frequencies
- **Channels Used**: MIDI Channel 2 → NES Triangle
- **Techniques**: Bass line patterns, sub-bass tones
- **Musical Content**: Walking bass line with typical NES patterns
- **Duration**: ~6 seconds

#### `noise_demo.mid` - Noise Channel
- **Purpose**: Shows percussion and sound effects using noise generation
- **Channels Used**: MIDI Channel 9 → NES Noise (standard drum channel)
- **Techniques**: Different noise periods for varied percussion sounds
- **Musical Content**: Drum pattern with kick, snare, and hi-hat equivalents
- **Duration**: ~4 seconds

#### `dmc_demo.mid` - Delta Modulation Channel
- **Purpose**: Demonstrates sample-based audio and low-frequency content
- **Channels Used**: MIDI Channel 3 → NES DMC
- **Techniques**: Sample playback simulation, bass enhancement
- **Musical Content**: Low-frequency melodic content and bass hits
- **Duration**: ~5 seconds

### 2. Technique Demonstrations (`techniques/`)

These files showcase advanced NES audio programming techniques and effects commonly used in chiptune music.

#### `arpeggios.mid` - Arpeggio Techniques
- **Purpose**: Fast note sequences creating chord impressions
- **Channels Used**: MIDI Channel 0 (Pulse 1)
- **Techniques**: Rapid note alternation, chord simulation
- **Musical Content**: Major chord arpeggios in sequence
- **NES Benefit**: Simulates polyphony on monophonic channels
- **Duration**: ~3 seconds

#### `pitch_slides.mid` - Pitch Bend Effects
- **Purpose**: Smooth frequency transitions and portamento effects
- **Channels Used**: MIDI Channel 0 (Pulse 1)
- **Techniques**: MIDI pitch bend, frequency sweeps
- **Musical Content**: Rising pitch slides and glissando effects
- **NES Benefit**: Demonstrates hardware pitch sweep capabilities
- **Duration**: ~3 seconds

#### `echo_effects.mid` - Echo and Delay
- **Purpose**: Spatial audio effects using multiple channels
- **Channels Used**: MIDI Channels 0 & 1 (Pulse 1 & 2)
- **Techniques**: Delayed note repetition, volume reduction
- **Musical Content**: Echo effects with decreasing volume
- **NES Benefit**: Creates illusion of reverb/delay without hardware support
- **Duration**: ~4 seconds

### 3. Musical Compositions (`compositions/`)

Complete musical pieces that demonstrate how to effectively combine NES channels for full compositions.

#### `simple_melody.mid` - Basic Melody with Bass
- **Purpose**: Simple but complete musical arrangement
- **Channels Used**:
  - MIDI Channel 0 → Pulse 1 (melody)
  - MIDI Channel 2 → Triangle (bass)
- **Techniques**: Melody-bass combination, traditional harmony
- **Musical Content**: "Twinkle Twinkle Little Star" style melody with bass line
- **NES Benefit**: Demonstrates basic two-voice arrangement
- **Duration**: ~8 seconds

#### `chiptune_style.mid` - Authentic Chiptune Composition
- **Purpose**: Fast-paced chiptune music showcasing typical patterns
- **Channels Used**:
  - MIDI Channel 0 → Pulse 1 (lead melody)
  - MIDI Channel 1 → Pulse 2 (harmony)
- **Techniques**: Rapid 16th note patterns, harmony doubling
- **Musical Content**: Energetic chiptune-style composition
- **NES Benefit**: Demonstrates authentic video game music style
- **Duration**: ~4 seconds

### 4. Educational Examples (`educational/`)

Reference materials for learning NES audio programming and understanding system capabilities.

#### `all_channels.mid` - Complete Channel Demonstration
- **Purpose**: Shows all 5 NES channels playing simultaneously
- **Channels Used**: All 5 MIDI channels mapped to NES channels
- **Techniques**: Multi-channel coordination, full system utilization
- **Musical Content**: Simple patterns on each channel simultaneously
- **NES Benefit**: Demonstrates maximum polyphony and channel interaction
- **Duration**: ~4 seconds

#### `note_range.mid` - NES Frequency Range Test
- **Purpose**: Demonstrates the full frequency range of NES channels
- **Channels Used**: MIDI Channel 0 (Pulse 1)
- **Techniques**: Chromatic scale across NES frequency range
- **Musical Content**: Ascending chromatic notes from low to high
- **NES Benefit**: Shows practical frequency limits and sweet spots
- **Duration**: ~6 seconds

## Channel Mapping Reference

The examples use standard MIDI channel mapping to NES channels:

| MIDI Channel | NES Channel | Characteristics |
|-------------|-------------|-----------------|
| 0 | Pulse 1 | Primary melody, lead sounds |
| 1 | Pulse 2 | Harmony, secondary melody |
| 2 | Triangle | Bass line, sub-bass |
| 3 | DMC | Samples, bass enhancement |
| 9 | Noise | Percussion, sound effects |

## Technical Specifications

### MIDI Format Details
- **Format**: Standard MIDI File (SMF) Format 0 or 1
- **Timing**: 480 ticks per quarter note
- **Tempo**: 120 BPM (default)
- **Note Range**: MIDI notes 24-108 (optimized for NES frequency range)

### NES Hardware Considerations
- **Pulse Channels**: Support 4 duty cycle settings (12.5%, 25%, 50%, 75%)
- **Triangle Channel**: Fixed volume, best for bass and sub-bass
- **Noise Channel**: 16 different noise periods, ideal for percussion
- **DMC Channel**: Sample-based, typically used for drums and bass enhancement
- **Frequency Range**: ~54 Hz to ~12.4 kHz (hardware limits)

## Usage Instructions

### Playing Examples
```bash
# Play individual examples
./mame_synth examples/midi/basic/pulse1_demo.mid

# Play with specific configuration
./mame_synth --config examples/config/authentic.json examples/midi/compositions/simple_melody.mid

# Output to file for analysis
./mame_synth --output examples_output.wav examples/midi/educational/all_channels.mid
```

### CLI Testing
```bash
# Load and configure
config-load examples/config/creative.json
load examples/midi/compositions/chiptune_style.mid

# Real-time control during playback
play
cv 0 0.8    # Adjust pulse 1 volume
cp 1 -0.3   # Pan pulse 2 left
pd 0 2      # Change pulse 1 duty cycle
```

### Analysis and Learning
```bash
# View file information
info examples/midi/techniques/arpeggios.mid

# Analyze NES optimization
analyze examples/midi/compositions/simple_melody.mid

# Validate NES compatibility
validate examples/midi/educational/all_channels.mid
```

## Example Generation

The examples are generated programmatically using the `generate_nes_examples` tool:

```bash
# Regenerate all examples
make generate_examples

# Or manually:
./build/tools/generate_nes_examples
```

This ensures accuracy and allows for easy modification of examples as the synthesizer evolves.

## Learning Path

### Beginner (Understanding NES Audio)
1. **Start with**: `basic/pulse1_demo.mid` - Learn primary melody channel
2. **Then try**: `basic/triangle_demo.mid` - Understand bass channel
3. **Next**: `educational/all_channels.mid` - See all channels together
4. **Practice**: `compositions/simple_melody.mid` - Basic two-voice music

### Intermediate (NES Techniques)
1. **Study**: `techniques/arpeggios.mid` - Learn chord simulation
2. **Experiment**: `techniques/echo_effects.mid` - Multi-channel effects
3. **Analyze**: `techniques/pitch_slides.mid` - Frequency modulation
4. **Create**: Modify existing examples for practice

### Advanced (Composition)
1. **Examine**: `compositions/chiptune_style.mid` - Fast-paced techniques
2. **Reference**: `educational/note_range.mid` - Frequency limitations
3. **Combine**: Multiple techniques in original compositions
4. **Optimize**: Use NES-specific configuration presets

## Best Practices Demonstrated

### Channel Assignment Strategy
- **Pulse 1**: Primary melody, lead instruments
- **Pulse 2**: Harmony, counter-melody, arpeggios
- **Triangle**: Bass line, sub-bass, occasionally melody
- **Noise**: Percussion, sound effects, texture
- **DMC**: Kick drums, bass enhancement, special effects

### Frequency Range Optimization
- **High frequencies (>2kHz)**: Pulse channels for clarity
- **Mid frequencies (200Hz-2kHz)**: All channels suitable
- **Low frequencies (<200Hz)**: Triangle and DMC channels
- **Sub-bass (<80Hz)**: Primarily triangle channel

### Timing and Rhythm
- **Fast passages**: Best on pulse channels
- **Sustained notes**: Triangle channel excels
- **Rhythmic elements**: Noise channel for percussion
- **Accent notes**: DMC for emphasis

### Volume Management
- **Triangle**: Fixed volume, balance with note density
- **Pulse channels**: Use velocity for dynamics
- **Noise**: Volume critical for percussion balance
- **DMC**: Often used for accents and fills

## Advanced Techniques

### Polyphony Simulation
- **Arpeggios**: Rapid note alternation creates chord impression
- **Channel doubling**: Same melody on multiple channels
- **Echo effects**: Delayed repetition adds density

### Timbral Variation
- **Duty cycle changes**: Alter pulse wave character
- **Noise periods**: Different percussion sounds
- **Volume envelopes**: Shape note attack and decay

### Spatial Effects
- **Pan positioning**: Stereo image with dual pulse channels
- **Echo delays**: Create sense of space
- **Volume automation**: Dynamic movement

## File Format Compatibility

All examples are standard MIDI files compatible with:
- **NES Synthesizer**: Primary target platform
- **Standard MIDI players**: For reference and comparison
- **Digital Audio Workstations**: For analysis and modification
- **Chiptune software**: For cross-platform compatibility

## Contributing New Examples

When creating new examples:

1. **Follow naming convention**: `category_description.mid`
2. **Use appropriate channels**: Follow established mapping
3. **Include documentation**: Update this README
4. **Test thoroughly**: Verify NES compatibility
5. **Consider learning value**: Educational benefit for users

## Troubleshooting

### Common Issues
- **No sound on triangle**: Check low-frequency content and volume balance
- **Distorted noise**: Verify noise channel note mapping (MIDI channel 9)
- **Missing bass**: Ensure triangle channel has appropriate low frequencies
- **Thin sound**: Use multiple channels for fuller arrangements

### Optimization Tips
- **Frequency range**: Stay within NES hardware limits
- **Channel usage**: Utilize all available channels effectively
- **Timing precision**: Use appropriate MIDI timing resolution
- **Volume balance**: Consider hardware channel characteristics

These examples provide a comprehensive foundation for understanding and utilizing the NES synthesizer's capabilities, from basic channel operation through advanced musical composition techniques.