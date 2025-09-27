#include "nes_channel_assignment.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <set>
#include <iomanip>

namespace nes_channel_assignment {

// Assignment strategy default initialization
void assignment_strategy::initialize_defaults() {
    // Initialize role-to-channel preferences (0.0 = poor, 1.0 = excellent)
    role_channel_preferences[track_analysis::musical_role::MELODY] = {0.9f, 0.7f, 0.3f, 0.1f, 0.2f};
    role_channel_preferences[track_analysis::musical_role::HARMONY] = {0.7f, 0.9f, 0.4f, 0.1f, 0.3f};
    role_channel_preferences[track_analysis::musical_role::BASS] = {0.2f, 0.2f, 0.9f, 0.1f, 0.7f};
    role_channel_preferences[track_analysis::musical_role::PERCUSSION] = {0.1f, 0.1f, 0.1f, 1.0f, 0.6f};
    role_channel_preferences[track_analysis::musical_role::ARPEGGIO] = {0.8f, 0.8f, 0.5f, 0.1f, 0.2f};
    role_channel_preferences[track_analysis::musical_role::PAD] = {0.6f, 0.7f, 0.8f, 0.1f, 0.4f};
    role_channel_preferences[track_analysis::musical_role::COUNTER_MELODY] = {0.6f, 0.8f, 0.5f, 0.1f, 0.3f};
    role_channel_preferences[track_analysis::musical_role::ACCENT] = {0.5f, 0.5f, 0.3f, 0.8f, 0.9f};
    role_channel_preferences[track_analysis::musical_role::SPECIAL_EFFECT] = {0.4f, 0.4f, 0.3f, 0.9f, 0.8f};

    // Initialize frequency ranges for each NES channel (Hz)
    channel_frequency_ranges = {
        {80.0f, 4000.0f, 800.0f},    // Pulse 1: Good mid-high range
        {80.0f, 4000.0f, 600.0f},    // Pulse 2: Good mid range
        {27.0f, 2000.0f, 150.0f},    // Triangle: Excellent for bass
        {50.0f, 20000.0f, 1000.0f},  // Noise: Full spectrum
        {50.0f, 8000.0f, 200.0f}     // DMC: Good for low-mid range
    };
}

// Channel assignment engine implementation
channel_assignment_engine::channel_assignment_engine() {
    // Initialize default strategies
    add_strategy(strategies::authentic_nes());
    add_strategy(strategies::modern_chiptune());
    add_strategy(strategies::quality_focused());
    add_strategy(strategies::creative_freedom());

    // Set default channel priority (most versatile first)
    m_channel_priority = {
        nes_channel_type::PULSE_1,
        nes_channel_type::PULSE_2,
        nes_channel_type::TRIANGLE,
        nes_channel_type::DMC,
        nes_channel_type::NOISE
    };
}

void channel_assignment_engine::add_strategy(const assignment_strategy& strategy) {
    m_strategies[strategy.name] = strategy;
}

void channel_assignment_engine::set_active_strategy(const std::string& name) {
    if (m_strategies.find(name) != m_strategies.end()) {
        m_active_strategy = name;
    }
}

const assignment_strategy* channel_assignment_engine::get_active_strategy() const {
    auto it = m_strategies.find(m_active_strategy);
    return (it != m_strategies.end()) ? &it->second : nullptr;
}

std::vector<std::string> channel_assignment_engine::get_strategy_names() const {
    std::vector<std::string> names;
    for (const auto& pair : m_strategies) {
        names.push_back(pair.first);
    }
    return names;
}

// Track analysis implementation
track_analysis channel_assignment_engine::analyze_track(const music_data& music, uint8_t midi_channel) const {
    track_analysis analysis;
    analysis.midi_channel = midi_channel;

    // Collect notes for this channel
    std::vector<music_note> channel_notes;
    for (const auto& note : music.notes()) {
        if (note.channel == midi_channel) {
            channel_notes.push_back(note);
        }
    }

    if (channel_notes.empty()) {
        return analysis; // Return empty analysis
    }

    analysis.note_count = channel_notes.size();

    // Frequency analysis
    std::vector<float> frequencies;
    std::vector<float> velocities;
    std::vector<float> durations;

    for (const auto& note : channel_notes) {
        float freq = midi_note_to_frequency(note.note);
        frequencies.push_back(freq);
        velocities.push_back(note.velocity);
        durations.push_back(note.duration);
    }

    // Calculate frequency statistics
    analysis.min_frequency_hz = *std::min_element(frequencies.begin(), frequencies.end());
    analysis.max_frequency_hz = *std::max_element(frequencies.begin(), frequencies.end());
    analysis.avg_frequency_hz = std::accumulate(frequencies.begin(), frequencies.end(), 0.0f) / frequencies.size();
    analysis.frequency_range_hz = analysis.max_frequency_hz - analysis.min_frequency_hz;

    // Calculate timing statistics
    analysis.avg_note_duration = std::accumulate(durations.begin(), durations.end(), 0.0f) / durations.size();
    analysis.has_sustained_notes = std::any_of(durations.begin(), durations.end(),
                                              [](float dur) { return dur > 1920; }); // > 4 beats at 480 tpq
    analysis.has_fast_passages = std::any_of(durations.begin(), durations.end(),
                                            [](float dur) { return dur < 120; }); // < 1/4 beat

    // Calculate note density (notes per second, approximate)
    if (!channel_notes.empty()) {
        music_time_t total_time = std::max_element(channel_notes.begin(), channel_notes.end(),
                                                  [](const music_note& a, const music_note& b) {
                                                      return (a.start + a.duration) < (b.start + b.duration);
                                                  })->start +
                                 std::max_element(channel_notes.begin(), channel_notes.end(),
                                                  [](const music_note& a, const music_note& b) {
                                                      return (a.start + a.duration) < (b.start + b.duration);
                                                  })->duration;

        analysis.notes_per_second = (float)analysis.note_count / (total_time / 480.0f / 2.0f); // Rough conversion
    }

    // Velocity analysis
    analysis.avg_velocity = std::accumulate(velocities.begin(), velocities.end(), 0.0f) / velocities.size();
    float min_vel = *std::min_element(velocities.begin(), velocities.end());
    float max_vel = *std::max_element(velocities.begin(), velocities.end());
    analysis.velocity_range = max_vel - min_vel;
    analysis.has_dynamic_variation = analysis.velocity_range > 40; // Significant velocity range

    // Detect musical role
    analysis.role = detect_musical_role(analysis);

    // Calculate channel suitability scores
    analysis.pulse1_suitability = calculate_frequency_suitability(analysis, nes_channel_type::PULSE_1) * 0.4f +
                                  calculate_role_suitability(analysis, nes_channel_type::PULSE_1) * 0.6f;

    analysis.pulse2_suitability = calculate_frequency_suitability(analysis, nes_channel_type::PULSE_2) * 0.4f +
                                  calculate_role_suitability(analysis, nes_channel_type::PULSE_2) * 0.6f;

    analysis.triangle_suitability = calculate_frequency_suitability(analysis, nes_channel_type::TRIANGLE) * 0.6f +
                                   calculate_role_suitability(analysis, nes_channel_type::TRIANGLE) * 0.4f;

    analysis.noise_suitability = calculate_role_suitability(analysis, nes_channel_type::NOISE);

    analysis.dmc_suitability = calculate_frequency_suitability(analysis, nes_channel_type::DMC) * 0.5f +
                              calculate_role_suitability(analysis, nes_channel_type::DMC) * 0.5f;

    return analysis;
}

std::vector<track_analysis> channel_assignment_engine::analyze_all_tracks(const music_data& music) const {
    std::vector<track_analysis> analyses;

    // Find all unique MIDI channels in the music
    std::set<uint8_t> used_channels;
    for (const auto& note : music.notes()) {
        used_channels.insert(note.channel);
    }

    // Analyze each used channel
    for (uint8_t channel : used_channels) {
        track_analysis analysis = analyze_track(music, channel);
        if (analysis.note_count > 0) {
            analyses.push_back(analysis);
        }
    }

    return analyses;
}

// Channel assignment algorithms
assignment_result channel_assignment_engine::assign_channels(const music_data& music) const {
    std::vector<track_analysis> analyses = analyze_all_tracks(music);
    return assign_channels(analyses);
}

assignment_result channel_assignment_engine::assign_channels(const std::vector<track_analysis>& analyses) const {
    const assignment_strategy* strategy = get_active_strategy();
    if (!strategy) {
        assignment_result result;
        result.warnings.push_back("No active assignment strategy found");
        return result;
    }

    // Use balanced assignment algorithm for best results
    assignment_result result = assign_balanced(analyses);

    // Resolve any conflicts
    resolve_assignment_conflicts(result, analyses);

    // Calculate overall quality score
    result.overall_quality_score = validate_assignment(result, analyses);

    // Generate warnings and suggestions
    if (result.overall_quality_score < 0.7f) {
        result.warnings.push_back("Assignment quality is suboptimal");
    }

    if (analyses.size() > 5) {
        result.warnings.push_back("More MIDI tracks than NES channels - some tracks will be merged or omitted");
    }

    return result;
}

assignment_result channel_assignment_engine::assign_balanced(const std::vector<track_analysis>& analyses) const {
    assignment_result result;
    const assignment_strategy* strategy = get_active_strategy();

    if (!strategy) return result;

    // Create sorted list of tracks by priority
    std::vector<std::pair<size_t, float>> track_priorities;

    for (size_t i = 0; i < analyses.size(); ++i) {
        const auto& analysis = analyses[i];

        // Calculate priority based on role importance and track quality
        float priority = 0.0f;

        switch (analysis.role) {
            case track_analysis::musical_role::MELODY:
                priority = 1.0f;
                break;
            case track_analysis::musical_role::BASS:
                priority = 0.9f;
                break;
            case track_analysis::musical_role::HARMONY:
                priority = 0.8f;
                break;
            case track_analysis::musical_role::PERCUSSION:
                priority = 0.7f;
                break;
            case track_analysis::musical_role::COUNTER_MELODY:
                priority = 0.6f;
                break;
            default:
                priority = 0.5f;
                break;
        }

        // Boost priority for tracks with more notes
        priority += std::min(0.2f, analysis.note_count / 100.0f);

        track_priorities.emplace_back(i, priority);
    }

    // Sort by priority (highest first)
    std::sort(track_priorities.begin(), track_priorities.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Track which NES channels are already assigned
    std::vector<bool> channel_used(5, false);

    // Assign channels in priority order
    for (const auto& track_priority : track_priorities) {
        size_t track_idx = track_priority.first;
        const auto& analysis = analyses[track_idx];

        // Find best available channel
        nes_channel_type best_channel = nes_channel_type::PULSE_1;
        float best_score = -1.0f;
        std::string best_reason;

        std::vector<float> channel_scores = {
            analysis.pulse1_suitability,
            analysis.pulse2_suitability,
            analysis.triangle_suitability,
            analysis.noise_suitability,
            analysis.dmc_suitability
        };

        for (int ch = 0; ch < 5; ++ch) {
            if (channel_used[ch]) continue;

            nes_channel_type channel = static_cast<nes_channel_type>(ch);
            float score = channel_scores[ch];

            // Apply strategy-specific bonuses
            if (analysis.role == track_analysis::musical_role::PERCUSSION && ch == 3) {
                score += 0.3f; // Bonus for assigning drums to noise
            }
            if (analysis.role == track_analysis::musical_role::BASS && ch == 2) {
                score += 0.2f; // Bonus for assigning bass to triangle
            }

            if (score > best_score) {
                best_score = score;
                best_channel = channel;

                // Generate reason
                std::stringstream reason;
                reason << "Best match for " << static_cast<int>(analysis.role)
                       << " role (score: " << std::fixed << std::setprecision(2) << score << ")";
                best_reason = reason.str();
            }
        }

        // Assign the best channel
        if (best_score >= 0.0f) {
            channel_used[static_cast<int>(best_channel)] = true;
            result.assignments.emplace_back(analysis.midi_channel, best_channel, best_score, best_reason);
        } else {
            result.warnings.push_back("Could not find suitable channel for MIDI channel " +
                                    std::to_string(analysis.midi_channel));
        }
    }

    return result;
}

assignment_result channel_assignment_engine::assign_greedy(const std::vector<track_analysis>& analyses) const {
    assignment_result result;

    // Simple greedy assignment - assign each track to its best channel if available
    std::vector<bool> channel_used(5, false);

    for (const auto& analysis : analyses) {
        std::vector<std::pair<nes_channel_type, float>> channel_scores = {
            {nes_channel_type::PULSE_1, analysis.pulse1_suitability},
            {nes_channel_type::PULSE_2, analysis.pulse2_suitability},
            {nes_channel_type::TRIANGLE, analysis.triangle_suitability},
            {nes_channel_type::NOISE, analysis.noise_suitability},
            {nes_channel_type::DMC, analysis.dmc_suitability}
        };

        // Sort by score (highest first)
        std::sort(channel_scores.begin(), channel_scores.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        // Assign to best available channel
        bool assigned = false;
        for (const auto& channel_score : channel_scores) {
            int ch_idx = static_cast<int>(channel_score.first);
            if (!channel_used[ch_idx]) {
                channel_used[ch_idx] = true;
                result.assignments.emplace_back(analysis.midi_channel, channel_score.first,
                                              channel_score.second, "Greedy assignment");
                assigned = true;
                break;
            }
        }

        if (!assigned) {
            result.warnings.push_back("Could not assign MIDI channel " + std::to_string(analysis.midi_channel));
        }
    }

    return result;
}

assignment_result channel_assignment_engine::assign_optimal(const std::vector<track_analysis>& analyses) const {
    // For now, use balanced assignment - could implement Hungarian algorithm for true optimality
    return assign_balanced(analyses);
}

// Musical role detection
track_analysis::musical_role channel_assignment_engine::detect_musical_role(const track_analysis& analysis) const {
    // Special case: MIDI channel 9 is typically drums
    if (analysis.midi_channel == 9) {
        return track_analysis::musical_role::PERCUSSION;
    }

    // Detect based on frequency and characteristics
    if (analysis.avg_frequency_hz < 150.0f && analysis.note_count > 10) {
        return track_analysis::musical_role::BASS;
    }

    if (analysis.has_fast_passages && analysis.frequency_range_hz > 1000.0f) {
        return track_analysis::musical_role::ARPEGGIO;
    }

    if (analysis.avg_frequency_hz > 400.0f && analysis.note_count > 20) {
        return track_analysis::musical_role::MELODY;
    }

    if (analysis.has_sustained_notes && analysis.velocity_range < 30) {
        return track_analysis::musical_role::PAD;
    }

    if (analysis.avg_frequency_hz > 300.0f && analysis.note_count > 5) {
        return track_analysis::musical_role::HARMONY;
    }

    return track_analysis::musical_role::UNKNOWN;
}

// Suitability calculation methods
float channel_assignment_engine::calculate_frequency_suitability(const track_analysis& analysis, nes_channel_type channel) const {
    const assignment_strategy* strategy = get_active_strategy();
    if (!strategy) return 0.5f;

    int ch_idx = static_cast<int>(channel);
    if (ch_idx >= strategy->channel_frequency_ranges.size()) return 0.5f;

    const auto& range = strategy->channel_frequency_ranges[ch_idx];

    // Calculate how well the track's frequencies fit the channel's optimal range
    float score = 0.0f;

    if (analysis.avg_frequency_hz >= range.min_hz && analysis.avg_frequency_hz <= range.max_hz) {
        // Within range - calculate how close to optimal
        float distance_from_optimal = std::abs(analysis.avg_frequency_hz - range.optimal_hz);
        float max_distance = std::max(range.optimal_hz - range.min_hz, range.max_hz - range.optimal_hz);
        score = 1.0f - (distance_from_optimal / max_distance);
    } else {
        // Out of range - penalize based on distance
        if (analysis.avg_frequency_hz < range.min_hz) {
            score = std::max(0.0f, 1.0f - (range.min_hz - analysis.avg_frequency_hz) / range.min_hz);
        } else {
            score = std::max(0.0f, 1.0f - (analysis.avg_frequency_hz - range.max_hz) / range.max_hz);
        }
    }

    return std::clamp(score, 0.0f, 1.0f);
}

float channel_assignment_engine::calculate_role_suitability(const track_analysis& analysis, nes_channel_type channel) const {
    const assignment_strategy* strategy = get_active_strategy();
    if (!strategy) return 0.5f;

    auto it = strategy->role_channel_preferences.find(analysis.role);
    if (it == strategy->role_channel_preferences.end()) {
        return 0.5f; // Default for unknown roles
    }

    int ch_idx = static_cast<int>(channel);
    if (ch_idx >= it->second.size()) return 0.5f;

    return it->second[ch_idx];
}

float channel_assignment_engine::calculate_density_suitability(const track_analysis& analysis, nes_channel_type channel) const {
    // Fast passages work better on pulse channels
    if (analysis.has_fast_passages) {
        switch (channel) {
            case nes_channel_type::PULSE_1:
            case nes_channel_type::PULSE_2:
                return 1.0f;
            case nes_channel_type::TRIANGLE:
                return 0.7f;
            default:
                return 0.3f;
        }
    }

    // Sustained notes work well on triangle
    if (analysis.has_sustained_notes && channel == nes_channel_type::TRIANGLE) {
        return 0.9f;
    }

    return 0.7f; // Default neutral score
}

float channel_assignment_engine::calculate_dynamics_suitability(const track_analysis& analysis, nes_channel_type channel) const {
    // Triangle has no volume control
    if (channel == nes_channel_type::TRIANGLE && analysis.has_dynamic_variation) {
        return 0.3f;
    }

    // Other channels handle dynamics well
    return 0.8f;
}

// Utility methods
float channel_assignment_engine::midi_note_to_frequency(uint8_t note) const {
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

bool channel_assignment_engine::is_frequency_suitable_for_channel(float frequency_hz, nes_channel_type channel) const {
    const assignment_strategy* strategy = get_active_strategy();
    if (!strategy) return true;

    int ch_idx = static_cast<int>(channel);
    if (ch_idx >= strategy->channel_frequency_ranges.size()) return true;

    const auto& range = strategy->channel_frequency_ranges[ch_idx];
    return frequency_hz >= range.min_hz && frequency_hz <= range.max_hz;
}

float channel_assignment_engine::validate_assignment(const assignment_result& result,
                                                    const std::vector<track_analysis>& analyses) const {
    if (result.assignments.empty()) return 0.0f;

    float total_score = 0.0f;

    for (const auto& assignment : result.assignments) {
        total_score += assignment.confidence_score;
    }

    return total_score / result.assignments.size();
}

void channel_assignment_engine::resolve_assignment_conflicts(assignment_result& result,
                                                           const std::vector<track_analysis>& analyses) const {
    // Check for duplicate channel assignments
    std::map<nes_channel_type, std::vector<size_t>> channel_assignments;

    for (size_t i = 0; i < result.assignments.size(); ++i) {
        channel_assignments[result.assignments[i].nes_channel].push_back(i);
    }

    // Resolve conflicts by reassigning lower-priority tracks
    for (auto& pair : channel_assignments) {
        if (pair.second.size() > 1) {
            // Keep the highest-scoring assignment
            auto best_it = std::max_element(pair.second.begin(), pair.second.end(),
                                          [&result](size_t a, size_t b) {
                                              return result.assignments[a].confidence_score <
                                                     result.assignments[b].confidence_score;
                                          });

            // Remove other assignments
            for (auto it = pair.second.begin(); it != pair.second.end(); ++it) {
                if (it != best_it) {
                    result.assignments.erase(result.assignments.begin() + *it);
                    result.warnings.push_back("Resolved channel conflict for MIDI channel " +
                                            std::to_string(result.assignments[*it].midi_channel));
                }
            }
        }
    }
}

// Predefined strategies
namespace strategies {

assignment_strategy authentic_nes() {
    assignment_strategy strategy("authentic_nes");
    strategy.description = "Mimics original NES game music channel usage patterns";
    strategy.frequency_weight = 0.3f;
    strategy.role_weight = 0.5f;
    strategy.density_weight = 0.1f;
    strategy.dynamics_weight = 0.1f;
    strategy.prefer_triangle_for_bass = true;
    strategy.auto_detect_drums = true;
    strategy.allow_multiple_melodies = false;
    return strategy;
}

assignment_strategy modern_chiptune() {
    assignment_strategy strategy("modern_chiptune");
    strategy.description = "Optimized for modern chiptune production";
    strategy.frequency_weight = 0.2f;
    strategy.role_weight = 0.3f;
    strategy.density_weight = 0.3f;
    strategy.dynamics_weight = 0.2f;
    strategy.allow_multiple_melodies = true;
    strategy.balance_pulse_channels = true;
    return strategy;
}

assignment_strategy quality_focused() {
    assignment_strategy strategy("quality_focused");
    strategy.description = "Prioritizes audio quality and frequency optimization";
    strategy.frequency_weight = 0.5f;
    strategy.role_weight = 0.2f;
    strategy.density_weight = 0.2f;
    strategy.dynamics_weight = 0.1f;
    return strategy;
}

assignment_strategy creative_freedom() {
    assignment_strategy strategy("creative_freedom");
    strategy.description = "Maximum flexibility for experimental compositions";
    strategy.frequency_weight = 0.15f;
    strategy.role_weight = 0.15f;
    strategy.density_weight = 0.35f;
    strategy.dynamics_weight = 0.35f;
    strategy.allow_multiple_melodies = true;
    strategy.prefer_triangle_for_bass = false;
    strategy.auto_detect_drums = false;
    return strategy;
}

} // namespace strategies

// Integration helpers - commented out to avoid dependency on nes_sequencer.h
// void apply_assignment_to_sequencer(nes_sequencer& sequencer, const assignment_result& assignment) {
//     std::vector<nes_sequencer::nes_channel_mapping> mapping = convert_to_sequencer_mapping(assignment);
//     sequencer.set_channel_mapping(mapping);
// }

// std::vector<nes_sequencer::nes_channel_mapping> convert_to_sequencer_mapping(const assignment_result& assignment) {
//     std::vector<nes_sequencer::nes_channel_mapping> mapping;

//     for (const auto& assign : assignment.assignments) {
//         mapping.emplace_back(assign.midi_channel, static_cast<uint8_t>(assign.nes_channel), true);
//     }

//     return mapping;
// }

} // namespace nes_channel_assignment