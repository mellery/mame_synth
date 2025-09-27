#include "nes_note_mapping.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace nes_note_mapping {

nes_note_mapper::nes_note_mapper(region_t region)
    : m_region(region), m_cpu_clock(region == region_t::NTSC ? NTSC_CPU_CLOCK : PAL_CPU_CLOCK) {
    initialize_lookup_tables();
}

uint16_t nes_note_mapper::note_to_timer(uint8_t note_number, channel_type_t channel_type) const {
    if (note_number >= 128) return MAX_TIMER_VALUE;

    switch (channel_type) {
        case channel_type_t::PULSE:
            return m_pulse_timer_table[note_number];
        case channel_type_t::TRIANGLE:
            return m_triangle_timer_table[note_number];
        case channel_type_t::NOISE:
            // Noise uses period table, not timer
            return note_to_noise_period(note_number);
    }
    return MAX_TIMER_VALUE;
}

double nes_note_mapper::timer_to_frequency(uint16_t timer, channel_type_t channel_type) const {
    if (timer == 0) return 0.0;

    uint8_t divider = (channel_type == channel_type_t::PULSE) ? PULSE_DIVIDER : TRIANGLE_DIVIDER;
    return static_cast<double>(m_cpu_clock) / (divider * (timer + 1));
}

double nes_note_mapper::note_to_frequency(uint8_t note_number) const {
    return midi_note_to_frequency(note_number);
}

bool nes_note_mapper::is_note_playable(uint8_t note_number, channel_type_t channel_type) const {
    if (note_number >= 128) return false;

    uint16_t timer = note_to_timer(note_number, channel_type);

    // Check if timer is in valid range
    if (timer < MIN_TIMER_VALUE || timer > MAX_TIMER_VALUE) {
        return false;
    }

    // Additional checks for channel-specific limitations
    switch (channel_type) {
        case channel_type_t::PULSE:
            // Pulse channels work well in mid-to-high frequency range
            return note_number >= 24 && note_number <= 108; // C1 to C8

        case channel_type_t::TRIANGLE:
            // Triangle channel works well across a wide range but avoid very high frequencies
            return note_number >= 12 && note_number <= 96; // C0 to C7

        case channel_type_t::NOISE:
            // Noise channel has limited period options
            return note_number >= 36 && note_number <= 84; // C2 to C6
    }

    return true;
}

nes_note_mapper::note_range nes_note_mapper::get_optimal_range(channel_type_t channel_type) const {
    switch (channel_type) {
        case channel_type_t::PULSE:
            return {36, 96}; // C2 to C7 - sweet spot for pulse channels

        case channel_type_t::TRIANGLE:
            return {24, 84}; // C1 to C6 - triangle works well for bass and mid-range

        case channel_type_t::NOISE:
            return {48, 72}; // C3 to C5 - limited range for percussive sounds
    }

    return {36, 84}; // Default range
}

uint8_t nes_note_mapper::note_to_noise_period(uint8_t note_number) const {
    // Map MIDI note to noise period index (0-15)
    // Higher notes -> shorter periods (higher indices)
    // Lower notes -> longer periods (lower indices)

    if (note_number < 36) return 0;  // Very low notes -> longest period
    if (note_number >= 108) return 15; // Very high notes -> shortest period

    // Map the usable range (36-107) to period indices (0-15)
    // Note 36 (C2) -> period 0, Note 107 (B7) -> period 15
    float normalized = (note_number - 36) / 71.0f; // 0.0 to 1.0
    uint8_t period_index = static_cast<uint8_t>(normalized * 15.0f + 0.5f);

    return std::min(period_index, static_cast<uint8_t>(15));
}

uint16_t nes_note_mapper::note_to_timer_with_bend(uint8_t note_number, channel_type_t channel_type,
                                                 int16_t pitch_bend_cents) const {
    // Convert pitch bend from cents to frequency multiplier
    // 100 cents = 1 semitone, 1200 cents = 1 octave
    double bend_factor = std::pow(2.0, pitch_bend_cents / 1200.0);

    double base_frequency = midi_note_to_frequency(note_number);
    double bent_frequency = base_frequency * bend_factor;

    uint8_t divider = (channel_type == channel_type_t::PULSE) ? PULSE_DIVIDER : TRIANGLE_DIVIDER;
    return frequency_to_timer(bent_frequency, divider);
}

const char* nes_note_mapper::note_to_name(uint8_t note_number) const {
    if (note_number >= 128) return "Invalid";
    return note_tables::NOTE_NAMES[note_number];
}

void nes_note_mapper::initialize_lookup_tables() {
    for (int note = 0; note < 128; ++note) {
        double frequency = midi_note_to_frequency(note);

        m_pulse_timer_table[note] = frequency_to_timer(frequency, PULSE_DIVIDER);
        m_triangle_timer_table[note] = frequency_to_timer(frequency, TRIANGLE_DIVIDER);
    }
}

double nes_note_mapper::midi_note_to_frequency(uint8_t note_number) const {
    // Standard MIDI tuning: A4 (note 69) = 440 Hz
    return 440.0 * std::pow(2.0, (note_number - 69) / 12.0);
}

uint16_t nes_note_mapper::frequency_to_timer(double frequency, uint8_t divider) const {
    if (frequency <= 0.0) return MAX_TIMER_VALUE;

    // Timer = (CPU_CLOCK / (divider * frequency)) - 1
    double timer_exact = (static_cast<double>(m_cpu_clock) / (divider * frequency)) - 1.0;
    uint32_t timer = static_cast<uint32_t>(timer_exact + 0.5); // Round to nearest

    return clamp_timer(static_cast<uint16_t>(timer));
}

uint16_t nes_note_mapper::clamp_timer(uint16_t timer) const {
    return std::clamp(timer, MIN_TIMER_VALUE, MAX_TIMER_VALUE);
}

// Pre-calculated note tables implementation
namespace note_tables {

// Generate MIDI frequency table
std::array<double, 128> generate_midi_frequencies() {
    std::array<double, 128> frequencies{};
    for (int note = 0; note < 128; ++note) {
        frequencies[note] = 440.0 * std::pow(2.0, (note - 69) / 12.0);
    }
    return frequencies;
}

// Generate NES timer tables
template<uint8_t DIVIDER>
std::array<uint16_t, 128> generate_nes_timers(uint32_t cpu_clock) {
    std::array<uint16_t, 128> timers{};
    for (int note = 0; note < 128; ++note) {
        double frequency = 440.0 * std::pow(2.0, (note - 69) / 12.0);
        double timer_exact = (static_cast<double>(cpu_clock) / (DIVIDER * frequency)) - 1.0;
        uint32_t timer = static_cast<uint32_t>(timer_exact + 0.5);
        timers[note] = std::clamp(static_cast<uint16_t>(timer), MIN_TIMER_VALUE, MAX_TIMER_VALUE);
    }
    return timers;
}

// Generate note names
std::array<const char*, 128> generate_note_names() {
    std::array<const char*, 128> names{};
    static const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static char name_buffer[128][8]; // Static storage for generated names

    for (int note = 0; note < 128; ++note) {
        int octave = (note / 12) - 1; // MIDI note 60 = C4
        int semitone = note % 12;
        snprintf(name_buffer[note], sizeof(name_buffer[note]), "%s%d", note_names[semitone], octave);
        names[note] = name_buffer[note];
    }
    return names;
}

// Initialize static tables
const std::array<double, 128> MIDI_FREQUENCIES = generate_midi_frequencies();
const std::array<uint16_t, 128> NTSC_PULSE_TIMERS = generate_nes_timers<PULSE_DIVIDER>(NTSC_CPU_CLOCK);
const std::array<uint16_t, 128> NTSC_TRIANGLE_TIMERS = generate_nes_timers<TRIANGLE_DIVIDER>(NTSC_CPU_CLOCK);
const std::array<uint16_t, 128> PAL_PULSE_TIMERS = generate_nes_timers<PULSE_DIVIDER>(PAL_CPU_CLOCK);
const std::array<uint16_t, 128> PAL_TRIANGLE_TIMERS = generate_nes_timers<TRIANGLE_DIVIDER>(PAL_CPU_CLOCK);
const std::array<const char*, 128> NOTE_NAMES = generate_note_names();

} // namespace note_tables

} // namespace nes_note_mapping