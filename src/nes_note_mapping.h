#pragma once

#include <cstdint>
#include <array>

/**
 * NES APU Note Mapping System
 *
 * Provides accurate note-to-frequency mapping for the NES APU,
 * taking into account hardware limitations and optimal frequency ranges.
 */
namespace nes_note_mapping {

// NES APU frequency constants
static constexpr uint32_t NTSC_CPU_CLOCK = 1789773;  // Hz
static constexpr uint32_t PAL_CPU_CLOCK = 1662607;   // Hz

static constexpr uint16_t MIN_TIMER_VALUE = 8;       // Minimum usable timer value
static constexpr uint16_t MAX_TIMER_VALUE = 2047;    // Maximum 11-bit timer value

// Channel-specific dividers
static constexpr uint8_t PULSE_DIVIDER = 16;
static constexpr uint8_t TRIANGLE_DIVIDER = 32;

// Noise period lookup table (matches NES hardware)
static constexpr std::array<uint16_t, 16> NOISE_PERIOD_TABLE = {{
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
}};

/**
 * NES APU Note Mapper
 * Handles conversion between MIDI notes and NES APU timer values
 */
class nes_note_mapper {
public:
    enum class region_t {
        NTSC,
        PAL
    };

    enum class channel_type_t {
        PULSE,
        TRIANGLE,
        NOISE
    };

    explicit nes_note_mapper(region_t region = region_t::NTSC);

    // Note to timer conversion
    uint16_t note_to_timer(uint8_t note_number, channel_type_t channel_type) const;

    // Timer to frequency conversion (for verification)
    double timer_to_frequency(uint16_t timer, channel_type_t channel_type) const;

    // Note to frequency conversion (direct)
    double note_to_frequency(uint8_t note_number) const;

    // Check if note is in playable range for given channel
    bool is_note_playable(uint8_t note_number, channel_type_t channel_type) const;

    // Get optimal octave range for channel
    struct note_range {
        uint8_t min_note;
        uint8_t max_note;
    };
    note_range get_optimal_range(channel_type_t channel_type) const;

    // Noise channel specific mapping
    uint8_t note_to_noise_period(uint8_t note_number) const;

    // Bend/detune support
    uint16_t note_to_timer_with_bend(uint8_t note_number, channel_type_t channel_type,
                                   int16_t pitch_bend_cents) const;

    // Utility functions
    uint32_t get_cpu_clock() const { return m_cpu_clock; }
    region_t get_region() const { return m_region; }

    // Get note name for debugging
    const char* note_to_name(uint8_t note_number) const;

private:
    region_t m_region;
    uint32_t m_cpu_clock;

    // Pre-calculated lookup tables for performance
    std::array<uint16_t, 128> m_pulse_timer_table;
    std::array<uint16_t, 128> m_triangle_timer_table;

    void initialize_lookup_tables();
    double midi_note_to_frequency(uint8_t note_number) const;
    uint16_t frequency_to_timer(double frequency, uint8_t divider) const;
    uint16_t clamp_timer(uint16_t timer) const;
};

/**
 * Pre-calculated note tables for common use cases
 */
namespace note_tables {
    // Standard 12-tone equal temperament MIDI note frequencies
    extern const std::array<double, 128> MIDI_FREQUENCIES;

    // NES APU timer values for pulse channels (NTSC)
    extern const std::array<uint16_t, 128> NTSC_PULSE_TIMERS;
    extern const std::array<uint16_t, 128> NTSC_TRIANGLE_TIMERS;

    // NES APU timer values for pulse channels (PAL)
    extern const std::array<uint16_t, 128> PAL_PULSE_TIMERS;
    extern const std::array<uint16_t, 128> PAL_TRIANGLE_TIMERS;

    // Note name strings for debugging
    extern const std::array<const char*, 128> NOTE_NAMES;
}

} // namespace nes_note_mapping