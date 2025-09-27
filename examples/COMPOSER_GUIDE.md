# NES Composer's Guide

A practical guide for musicians and composers creating music with the NES synthesizer, including composition techniques, workflow tips, and artistic considerations.

## Getting Started with NES Composition

### Understanding NES Limitations as Creative Constraints

The NES APU's limitations aren't restrictions—they're creative opportunities that define the iconic chiptune sound:

- **5 channels total**: Forces careful arrangement and voice leading
- **Monophonic channels**: Encourages arpeggio and melodic techniques
- **Fixed triangle volume**: Requires creative bass line writing
- **Limited noise tones**: Inspires creative percussion programming

### Essential Workflow Setup

#### 1. Configuration Selection
```bash
# For authentic NES sound
config-preset authentic

# For modern production quality
config-preset quality

# For creative freedom
config-preset creative
```

#### 2. Basic Playback Setup
```bash
# Start with a basic example
load examples/midi/basic/pulse1_demo.mid
play

# Experiment with real-time controls
cv 0 0.8    # Adjust volume
pd 0 2      # Change duty cycle
```

## Composition Techniques

### 1. Melody Writing for Pulse Channels

#### Primary Melody (Pulse 1)
- **Range**: C4 to C6 works best for lead melodies
- **Articulation**: Use staccato for clarity, legato for smoothness
- **Duty Cycle**: 50% for bright leads, 25% for mellow tones

**Example from `pulse1_demo.mid`:**
```
C4(100) → E4(80) → G4(60) → C5(40)
Different velocities demonstrate duty cycle variations
```

#### Harmony Parts (Pulse 2)
- **Range**: G3 to G5 for optimal harmony
- **Intervals**: 3rds, 5ths, and 6ths work well
- **Rhythm**: Can be more complex than melody

**Example from `pulse2_demo.mid`:**
```
Melody:  C4 - D4 - E4 - F4
Harmony: E4 - F4 - G4 - A4
Creates parallel 3rds harmony
```

### 2. Bass Line Composition (Triangle Channel)

#### Technical Considerations
- **No volume control**: Use note density and timing for dynamics
- **Frequency range**: C2 to C4 optimal, avoid extreme highs
- **Attack characteristics**: Instant attack, use for rhythmic precision

#### Bass Line Patterns

**Walking Bass (from `triangle_demo.mid`):**
```
C3 → G3 → A3 → F3 → C3 → E3 → F3 → G3
Each note quarter note duration, creates forward motion
```

**Pedal Tones:**
```
C3 sustained while harmony changes above
Provides harmonic foundation
```

**Rhythmic Bass:**
```
C3 (quarter) → rest (eighth) → C3 (eighth) → G3 (quarter)
Creates rhythmic interest without volume changes
```

### 3. Percussion Programming (Noise Channel)

#### Standard Drum Kit Mapping
- **MIDI Note 36 (C2)**: Kick drum (low noise)
- **MIDI Note 38 (D2)**: Snare drum (mid noise)
- **MIDI Note 42 (F#2)**: Hi-hat (high noise)
- **MIDI Note 44 (G#2)**: Pedal hi-hat
- **MIDI Note 46 (A#2)**: Open hi-hat

#### Pattern Examples

**Basic Rock Beat (from `noise_demo.mid`):**
```
Beat: 1   e   +   a   2   e   +   a   3   e   +   a   4   e   +   a
Kick: X   -   -   -   -   -   -   -   X   -   -   -   -   -   -   -
Snare:-   -   -   -   X   -   -   -   -   -   -   -   X   -   -   -
HH:   X   -   X   -   X   -   X   -   X   -   X   -   X   -   X   -
```

### 4. Advanced Techniques

#### Arpeggios for Polyphony Simulation

**Fast Arpeggios (from `arpeggios.mid`):**
```
C4 → E4 → G4 (repeat rapidly)
Creates impression of C major chord
Timing: 80 ticks per note (very fast)
```

**Broken Chord Patterns:**
```
C4 → G4 → E4 → G4 (ascending/descending)
More musical than straight arpeggios
```

#### Echo and Delay Effects

**Multi-Channel Echo (from `echo_effects.mid`):**
```
Channel 0: C5 at velocity 100 (original)
Channel 1: C5 at velocity 60, delayed 240 ticks (echo)
Channel 0: C5 at velocity 30, delayed 480 ticks (second echo)
```

#### Pitch Slides and Bends

**Sweep Effects (from `pitch_slides.mid`):**
```
Start: C4 with pitch bend 8192 (center)
Slide: Gradually increase pitch bend to 12288
Result: Smooth upward frequency sweep
```

## Arrangement Strategies

### 2-Channel Arrangements (Beginner)

**Melody + Bass:**
- Pulse 1: Primary melody
- Triangle: Bass line
- Example: `simple_melody.mid`

**Melody + Harmony:**
- Pulse 1: Lead melody
- Pulse 2: Harmony line
- Example: First part of `chiptune_style.mid`

### 3-Channel Arrangements (Intermediate)

**Full Harmonic Structure:**
- Pulse 1: Melody
- Pulse 2: Harmony/counter-melody
- Triangle: Bass line

**Rhythmic Interest:**
- Pulse 1: Melody
- Triangle: Bass
- Noise: Simple percussion

### 5-Channel Arrangements (Advanced)

**Complete Production (from `all_channels.mid`):**
- Pulse 1: Lead melody
- Pulse 2: Harmony/arpeggios
- Triangle: Bass line
- Noise: Full drum kit
- DMC: Bass enhancement/special effects

## Style-Specific Techniques

### Classic Video Game Style

**Characteristics:**
- Fast arpeggiated accompaniment
- Staccato melodies
- Simple harmonic progressions
- Prominent bass lines

**Example Progression:**
```
I - vi - IV - V (C - Am - F - G)
Works well with triangle bass and pulse harmony
```

### Modern Chiptune Style

**Characteristics:**
- Complex rhythmic patterns
- Extended harmonies
- Multiple melody layers
- Creative percussion

**Advanced Harmony:**
```
Add9 chords via arpeggios: C - E - G - D
Sus chords: C - F - G (no third)
```

### Ambient/Atmospheric

**Techniques:**
- Sustained triangle tones
- Slow pulse wave pads
- Minimal percussion
- Echo effects for space

## Mixing and Production Tips

### Volume Balance

**Starting Points:**
- Pulse 1 (melody): 100% velocity
- Pulse 2 (harmony): 70-80% velocity
- Triangle (bass): Natural volume (no control)
- Noise (drums): 60-90% depending on role
- DMC: 70-100% for accents

### Frequency Management

**EQ Considerations:**
- **Pulse channels**: Natural brightness, good for mid-high frequencies
- **Triangle**: Strong fundamental, excellent for bass frequencies
- **Noise**: Full spectrum, balance with musical content
- **DMC**: Often low-frequency content, use sparingly

### Spatial Effects

**Stereo Positioning:**
```bash
# Pan controls (range: -1.0 to 1.0)
cp 0 -0.3    # Pulse 1 slightly left
cp 1 0.3     # Pulse 2 slightly right
# Triangle and noise typically center
```

## Workflow Examples

### Starting a New Composition

1. **Choose a key and tempo**
   ```bash
   # Set up comfortable tempo
   mc tempo 1.0
   ```

2. **Start with bass line**
   ```
   Create simple bass pattern on triangle channel
   Use quarter or half notes for stability
   ```

3. **Add melody**
   ```
   Write melody on pulse 1
   Keep it simple initially
   ```

4. **Build arrangement**
   ```
   Add harmony on pulse 2
   Introduce percussion on noise channel
   ```

### Developing Ideas

**From Simple to Complex:**
1. Start with `simple_melody.mid` as template
2. Add complexity gradually
3. Use `chiptune_style.mid` techniques for energy
4. Reference `all_channels.mid` for full arrangements

### Creative Exercises

**Daily Practice:**
1. **Arpeggio Study**: Create new arpeggio patterns
2. **Bass Line Workshop**: Write bass lines in different styles
3. **Percussion Programming**: Experiment with noise channel rhythms
4. **Echo Experiments**: Try different delay timings and levels

## Common Composition Challenges

### Problem: Thin or Empty Sound
**Solutions:**
- Use all available channels
- Add arpeggiated accompaniment
- Layer melody with harmony
- Fill gaps with triangle channel activity

### Problem: Muddy Low End
**Solutions:**
- Keep triangle channel clear and defined
- Avoid overlapping triangle and DMC frequencies
- Use rhythmic variation instead of sustained bass

### Problem: Repetitive Percussion
**Solutions:**
- Vary velocity for dynamics
- Use different noise periods
- Create fill patterns
- Combine with triangle channel for hybrid rhythm

### Problem: Limited Harmonic Options
**Solutions:**
- Use arpeggios to imply complex chords
- Employ voice leading between channels
- Create harmonic rhythm with bass movement
- Use non-chord tones for interest

## Advanced Composition Concepts

### Voice Leading

**Smooth Motion:**
```
Pulse 1: C4 → D4 → E4
Pulse 2: E4 → F4 → G4
Parallel motion with smooth transitions
```

**Contrary Motion:**
```
Pulse 1: C4 → D4 → E4 (ascending)
Pulse 2: G4 → F4 → E4 (descending)
Creates independence and interest
```

### Rhythmic Displacement

**Offset Rhythms:**
```
Pulse 1: On beats 1 and 3
Pulse 2: On beats 2 and 4
Triangle: Steady quarter notes
Creates rhythmic complexity
```

### Motivic Development

**Transform Simple Ideas:**
1. Original motif: C4-D4-E4
2. Inversion: C4-B3-A3
3. Retrograde: E4-D4-C4
4. Augmentation: Longer note values

## Performance Considerations

### Live Performance Setup

**Real-Time Control:**
```bash
# Essential real-time controls
cv 0-2     # Channel volumes
cp 0-1     # Pan positions
pd 0-1     # Pulse duty cycles
mc volume  # Master volume
```

**Preset Management:**
```bash
# Save performance setup
realtime-preset save "live_setup"

# Quick recall during performance
realtime-preset load "live_setup"
```

### Recording Tips

**File Output:**
```bash
# High-quality recording
config-set audio.sample_rate 48000
config-set audio.buffer_size 512
./mame_synth --output recording.wav your_composition.mid
```

## Inspiration and References

### Study These Examples
1. **`pulse1_demo.mid`**: Pure melody writing
2. **`triangle_demo.mid`**: Bass line construction
3. **`arpeggios.mid`**: Harmonic complexity
4. **`echo_effects.mid`**: Spatial effects
5. **`chiptune_style.mid`**: Energy and drive
6. **`all_channels.mid`**: Full arrangement

### Analysis Exercises
- Transcribe classic NES game music
- Analyze harmony and voice leading
- Study rhythm and percussion patterns
- Examine arrangement techniques

### Creative Challenges
- **One-Channel Studies**: Write complete pieces using only one channel
- **Genre Exploration**: Adapt non-chiptune styles to NES limitations
- **Collaborative Composition**: Each composer takes specific channels
- **Rhythmic Focus**: Compose percussion-driven pieces

## Next Steps

### Skill Development Path

**Beginner → Intermediate:**
1. Master basic channel characteristics
2. Write simple two-channel arrangements
3. Develop arpeggio techniques
4. Learn basic percussion programming

**Intermediate → Advanced:**
1. Complex multi-channel arrangements
2. Advanced harmonic techniques
3. Live performance skills
4. Original style development

### Community and Resources

**Share Your Work:**
- Contribute new examples to the collection
- Document your techniques
- Collaborate with other composers
- Perform your compositions live

**Continue Learning:**
- Study NES game soundtracks
- Explore other chiptune platforms
- Experiment with hybrid styles
- Push the boundaries of NES sound

The NES synthesizer offers a unique combination of limitation and creativity. These constraints have inspired decades of memorable music and continue to offer fresh possibilities for contemporary composers. Use these examples as starting points for your own musical journey into the distinctive world of NES sound.