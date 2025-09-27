#pragma once

#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include <memory>
#include <functional>
#include "music_parser.h"

/**
 * Intelligent MIDI-to-NES channel assignment system
 *
 * Analyzes MIDI tracks and automatically assigns them to optimal NES channels
 * based on frequency content, musical role, and NES hardware characteristics.
 */

// Forward declarations
class nes_sequencer;

namespace nes_channel_assignment {

    // NES channel types with their characteristics
    enum class nes_channel_type {
        PULSE_1 = 0,    // Square wave, variable duty cycle, sweep
        PULSE_2 = 1,    // Square wave, variable duty cycle, sweep
        TRIANGLE = 2,   // Triangle wave, fixed volume
        NOISE = 3,      // Pseudo-random noise
        DMC = 4         // Delta modulation channel (samples)
    };

    // Track analysis data
    struct track_analysis {
        uint8_t midi_channel;

        // Frequency analysis
        float avg_frequency_hz;      // Average note frequency
        float min_frequency_hz;      // Lowest note frequency
        float max_frequency_hz;      // Highest note frequency
        float frequency_range_hz;    // Range of frequencies used

        // Note density and rhythm
        uint32_t note_count;         // Total number of notes
        float notes_per_second;      // Average note density
        float avg_note_duration;     // Average note duration in ticks
        bool has_sustained_notes;    // Has notes longer than 1 second
        bool has_fast_passages;      // Has rapid note sequences

        // Velocity and dynamics
        float avg_velocity;          // Average note velocity
        float velocity_range;        // Range of velocities used
        bool has_dynamic_variation;  // Uses wide velocity range

        // Musical role detection
        enum class musical_role {
            UNKNOWN,
            MELODY,           // Primary melodic line
            HARMONY,          // Harmonic support
            BASS,             // Bass line (low frequencies)
            PERCUSSION,       // Rhythmic/percussive (channel 9 or noise-like)
            ARPEGGIO,         // Fast arpeggiated patterns
            PAD,              // Sustained background
            COUNTER_MELODY,   // Secondary melodic line
            ACCENT,           // Accents and fills
            SPECIAL_EFFECT    // Sound effects or unique sounds
        } role;

        // Channel suitability scores (0.0 = poor, 1.0 = perfect)
        float pulse1_suitability;
        float pulse2_suitability;
        float triangle_suitability;
        float noise_suitability;
        float dmc_suitability;

        track_analysis() : midi_channel(0), avg_frequency_hz(0), min_frequency_hz(0),
                          max_frequency_hz(0), frequency_range_hz(0), note_count(0),
                          notes_per_second(0), avg_note_duration(0), has_sustained_notes(false),
                          has_fast_passages(false), avg_velocity(64), velocity_range(0),
                          has_dynamic_variation(false), role(musical_role::UNKNOWN),
                          pulse1_suitability(0), pulse2_suitability(0), triangle_suitability(0),
                          noise_suitability(0), dmc_suitability(0) {}
    };

    // Assignment strategy configuration
    struct assignment_strategy {
        std::string name;
        std::string description;

        // Strategy parameters
        float frequency_weight = 0.25f;      // Weight for frequency analysis
        float role_weight = 0.40f;           // Weight for musical role
        float density_weight = 0.20f;        // Weight for note density
        float dynamics_weight = 0.15f;       // Weight for dynamic range

        // Channel preferences by role
        std::map<track_analysis::musical_role, std::vector<float>> role_channel_preferences;

        // Frequency range preferences (Hz)
        struct frequency_range {
            float min_hz, max_hz, optimal_hz;
        };
        std::vector<frequency_range> channel_frequency_ranges;

        // Assignment constraints
        bool allow_multiple_melodies = false;    // Allow multiple tracks on pulse channels
        bool prefer_triangle_for_bass = true;    // Prefer triangle for bass frequencies
        bool auto_detect_drums = true;           // Auto-assign channel 9 to noise
        bool balance_pulse_channels = true;      // Balance load between pulse 1 & 2

        assignment_strategy(const std::string& n = "default") : name(n) {
            initialize_defaults();
        }

    private:
        void initialize_defaults();
    };

    // Assignment result
    struct assignment_result {
        struct channel_assignment {
            uint8_t midi_channel;
            nes_channel_type nes_channel;
            float confidence_score;      // 0.0-1.0 confidence in assignment
            std::string reason;          // Human-readable explanation

            channel_assignment(uint8_t midi_ch, nes_channel_type nes_ch, float score, const std::string& r)
                : midi_channel(midi_ch), nes_channel(nes_ch), confidence_score(score), reason(r) {}
        };

        std::vector<channel_assignment> assignments;
        float overall_quality_score;         // Overall assignment quality (0.0-1.0)
        std::vector<std::string> warnings;   // Potential issues with assignment
        std::vector<std::string> suggestions; // Optimization suggestions

        assignment_result() : overall_quality_score(0.0f) {}
    };

    // Channel assignment engine
    class channel_assignment_engine {
    public:
        channel_assignment_engine();
        ~channel_assignment_engine() = default;

        // Strategy management
        void add_strategy(const assignment_strategy& strategy);
        void set_active_strategy(const std::string& name);
        const assignment_strategy* get_active_strategy() const;
        std::vector<std::string> get_strategy_names() const;

        // Track analysis
        track_analysis analyze_track(const music_data& music, uint8_t midi_channel) const;
        std::vector<track_analysis> analyze_all_tracks(const music_data& music) const;

        // Channel assignment
        assignment_result assign_channels(const music_data& music) const;
        assignment_result assign_channels(const std::vector<track_analysis>& analyses) const;

        // Manual assignment helpers
        assignment_result assign_with_constraints(const music_data& music,
                                                 const std::map<uint8_t, nes_channel_type>& fixed_assignments) const;

        // Assignment validation and optimization
        float validate_assignment(const assignment_result& result,
                                 const std::vector<track_analysis>& analyses) const;
        assignment_result optimize_assignment(const assignment_result& initial_result,
                                            const std::vector<track_analysis>& analyses) const;

        // Configuration
        void set_channel_priority_order(const std::vector<nes_channel_type>& priority);
        void set_frequency_ranges(const std::vector<std::pair<float, float>>& ranges);

        // Statistics and reporting
        std::string generate_assignment_report(const assignment_result& result,
                                              const std::vector<track_analysis>& analyses) const;

    private:
        std::map<std::string, assignment_strategy> m_strategies;
        std::string m_active_strategy = "default";
        std::vector<nes_channel_type> m_channel_priority;

        // Analysis methods
        float calculate_frequency_suitability(const track_analysis& analysis, nes_channel_type channel) const;
        float calculate_role_suitability(const track_analysis& analysis, nes_channel_type channel) const;
        float calculate_density_suitability(const track_analysis& analysis, nes_channel_type channel) const;
        float calculate_dynamics_suitability(const track_analysis& analysis, nes_channel_type channel) const;

        // Musical role detection
        track_analysis::musical_role detect_musical_role(const track_analysis& analysis) const;

        // Assignment algorithms
        assignment_result assign_greedy(const std::vector<track_analysis>& analyses) const;
        assignment_result assign_optimal(const std::vector<track_analysis>& analyses) const;
        assignment_result assign_balanced(const std::vector<track_analysis>& analyses) const;

        // Utility methods
        float midi_note_to_frequency(uint8_t note) const;
        bool is_frequency_suitable_for_channel(float frequency_hz, nes_channel_type channel) const;
        float calculate_channel_load(const assignment_result& result, nes_channel_type channel) const;

        // Conflict resolution
        void resolve_assignment_conflicts(assignment_result& result,
                                        const std::vector<track_analysis>& analyses) const;
    };

    // Predefined assignment strategies
    namespace strategies {
        assignment_strategy authentic_nes();      // Mimics original NES game music
        assignment_strategy modern_chiptune();    // Optimized for modern chiptune
        assignment_strategy quality_focused();    // Prioritizes audio quality
        assignment_strategy creative_freedom();   // Maximum creative flexibility
        assignment_strategy bass_heavy();         // Optimized for bass-heavy music
        assignment_strategy melodic_focus();      // Optimized for complex melodies
    }

    // Assignment presets for common scenarios
    namespace presets {
        assignment_result simple_melody_bass();         // Melody + bass arrangement
        assignment_result full_chiptune();             // Complete 5-channel chiptune
        assignment_result percussion_focus();          // Rhythm-heavy arrangement
        assignment_result harmonic_complex();          // Complex harmonic arrangement
    }

    // Integration helpers (forward declarations only - implementation requires nes_sequencer.h)
    // void apply_assignment_to_sequencer(nes_sequencer& sequencer, const assignment_result& assignment);
    // std::vector<nes_sequencer::nes_channel_mapping> convert_to_sequencer_mapping(const assignment_result& assignment);

} // namespace nes_channel_assignment