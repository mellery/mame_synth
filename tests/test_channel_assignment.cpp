#include "test_framework_enhanced.h"
#include "../src/nes_channel_assignment.h"
#include <cmath>

using namespace nes_channel_assignment;

// Unit tests for channel assignment algorithms
REGISTER_TEST(channel_assignment, track_analysis_basic) {
    channel_assignment_engine engine;

    // Create simple test music data
    music_data music;
    music.add_note(music_note(0, 60, 100, 0, 480));    // C4
    music.add_note(music_note(0, 64, 90, 480, 480));   // E4
    music.add_note(music_note(0, 67, 80, 960, 480));   // G4

    track_analysis analysis = engine.analyze_track(music, 0);

    ASSERT_EQ(analysis.midi_channel, 0);
    ASSERT_EQ(analysis.note_count, 3);
    ASSERT_GT(analysis.avg_frequency_hz, 200.0f);
    ASSERT_LT(analysis.avg_frequency_hz, 500.0f);
    ASSERT_GT(analysis.pulse1_suitability, 0.0f);
    ASSERT_LT(analysis.pulse1_suitability, 1.0f);
}

REGISTER_TEST(channel_assignment, track_analysis_bass) {
    channel_assignment_engine engine;

    // Create bass-like music data
    music_data music;
    music.add_note(music_note(2, 36, 127, 0, 480));    // C2
    music.add_note(music_note(2, 43, 127, 480, 480));  // G2
    music.add_note(music_note(2, 41, 127, 960, 480));  // F2

    track_analysis analysis = engine.analyze_track(music, 2);

    ASSERT_EQ(analysis.midi_channel, 2);
    ASSERT_EQ(analysis.note_count, 3);
    ASSERT_LT(analysis.avg_frequency_hz, 150.0f);  // Bass frequencies
    ASSERT_GE(analysis.triangle_suitability, analysis.pulse1_suitability);  // Better or equal for triangle
}

REGISTER_TEST(channel_assignment, track_analysis_drums) {
    channel_assignment_engine engine;

    // Create drum-like music data (MIDI channel 9)
    music_data music;
    music.add_note(music_note(9, 36, 127, 0, 120));    // Kick
    music.add_note(music_note(9, 38, 100, 240, 120));  // Snare
    music.add_note(music_note(9, 42, 80, 480, 120));   // Hi-hat
    music.add_note(music_note(9, 42, 60, 600, 120));   // Hi-hat

    track_analysis analysis = engine.analyze_track(music, 9);

    ASSERT_EQ(analysis.midi_channel, 9);
    ASSERT_EQ(analysis.note_count, 4);
    ASSERT_GE(analysis.noise_suitability, 0.5f);  // Suitable for noise channel
}

REGISTER_TEST(channel_assignment, frequency_calculation) {
    channel_assignment_engine engine;

    // Test MIDI note to frequency conversion
    music_data music;
    music.add_note(music_note(0, 69, 100, 0, 480));    // A4 = 440 Hz

    track_analysis analysis = engine.analyze_track(music, 0);

    ASSERT_NEAR(analysis.avg_frequency_hz, 440.0f, 1.0f);
    ASSERT_NEAR(analysis.min_frequency_hz, 440.0f, 1.0f);
    ASSERT_NEAR(analysis.max_frequency_hz, 440.0f, 1.0f);
}

REGISTER_TEST(channel_assignment, strategy_management) {
    channel_assignment_engine engine;

    // Set a strategy first
    engine.set_active_strategy("modern_chiptune");

    // Test strategy switching
    ASSERT_TRUE(engine.get_active_strategy() != nullptr);

    engine.set_active_strategy("modern_chiptune");
    const assignment_strategy* strategy = engine.get_active_strategy();
    ASSERT_TRUE(strategy != nullptr);
    ASSERT_EQ(strategy->name, "modern_chiptune");

    // Test invalid strategy
    engine.set_active_strategy("invalid_strategy");
    // Should still have the previous valid strategy
    ASSERT_EQ(engine.get_active_strategy()->name, "modern_chiptune");
}

REGISTER_TEST(channel_assignment, assignment_basic) {
    channel_assignment_engine engine;
    engine.set_active_strategy("authentic_nes");

    // Create music with different channel types
    music_data music;

    // Melody on channel 0
    music.add_note(music_note(0, 72, 100, 0, 480));    // C5
    music.add_note(music_note(0, 74, 90, 480, 480));   // D5

    // Bass on channel 2
    music.add_note(music_note(2, 36, 127, 0, 480));    // C2
    music.add_note(music_note(2, 43, 127, 480, 480));  // G2

    // Drums on channel 9
    music.add_note(music_note(9, 36, 127, 0, 120));    // Kick
    music.add_note(music_note(9, 38, 100, 240, 120));  // Snare

    assignment_result result = engine.assign_channels(music);

    ASSERT_FALSE(result.assignments.empty());
    ASSERT_GT(result.overall_quality_score, 0.5f);

    // Check that drums are assigned to noise channel
    bool drums_to_noise = false;
    for (const auto& assignment : result.assignments) {
        if (assignment.midi_channel == 9 && assignment.nes_channel == nes_channel_type::NOISE) {
            drums_to_noise = true;
            break;
        }
    }
    ASSERT_TRUE(drums_to_noise);
}

REGISTER_TEST(channel_assignment, assignment_conflict_resolution) {
    channel_assignment_engine engine;

    // Create music with more tracks than NES channels
    music_data music;

    for (uint8_t ch = 0; ch < 8; ++ch) {
        music.add_note(music_note(ch, 60 + ch, 100, 0, 480));
    }

    assignment_result result = engine.assign_channels(music);

    // Should have at most 5 assignments (NES channel limit)
    ASSERT_LE(result.assignments.size(), 5);

    // Check for duplicate NES channel assignments
    std::set<nes_channel_type> used_channels;
    for (const auto& assignment : result.assignments) {
        ASSERT_TRUE(used_channels.find(assignment.nes_channel) == used_channels.end());
        used_channels.insert(assignment.nes_channel);
    }
}

REGISTER_TEST(channel_assignment, assignment_quality_scoring) {
    channel_assignment_engine engine;
    engine.set_active_strategy("modern_chiptune");

    // Create optimal assignment scenario
    music_data optimal_music;
    optimal_music.add_note(music_note(0, 72, 100, 0, 480));   // High melody -> Pulse 1
    optimal_music.add_note(music_note(2, 36, 127, 0, 480));   // Bass -> Triangle
    optimal_music.add_note(music_note(9, 36, 127, 0, 120));   // Drums -> Noise

    assignment_result optimal_result = engine.assign_channels(optimal_music);

    // Create suboptimal assignment scenario
    music_data suboptimal_music;
    for (uint8_t ch = 0; ch < 10; ++ch) {
        suboptimal_music.add_note(music_note(ch, 60, 100, 0, 480));
    }

    assignment_result suboptimal_result = engine.assign_channels(suboptimal_music);

    // Optimal scenario should have higher quality score
    ASSERT_GT(optimal_result.overall_quality_score, suboptimal_result.overall_quality_score);
}

REGISTER_TEST(channel_assignment, empty_music_handling) {
    channel_assignment_engine engine;

    music_data empty_music;
    assignment_result result = engine.assign_channels(empty_music);

    ASSERT_TRUE(result.assignments.empty());
    ASSERT_EQ(result.overall_quality_score, 0.0f);
}

REGISTER_TEST(channel_assignment, single_note_analysis) {
    channel_assignment_engine engine;

    music_data music;
    music.add_note(music_note(0, 69, 100, 0, 480));  // Single A4 note

    track_analysis analysis = engine.analyze_track(music, 0);

    ASSERT_EQ(analysis.note_count, 1);
    ASSERT_NEAR(analysis.avg_frequency_hz, 440.0f, 1.0f);
    ASSERT_EQ(analysis.frequency_range_hz, 0.0f);  // No range with single note
    ASSERT_FALSE(analysis.has_fast_passages);
    ASSERT_FALSE(analysis.has_sustained_notes);
}

// Performance tests for channel assignment
REGISTER_PERFORMANCE_TEST(channel_assignment, large_music_analysis) {
    static channel_assignment_engine engine;
    static music_data large_music = test_data_generator::generate_random_music(1000, 10000);

    // This should complete quickly even with large datasets
    engine.analyze_all_tracks(large_music);
}

REGISTER_PERFORMANCE_TEST(channel_assignment, assignment_speed) {
    static channel_assignment_engine engine;
    static music_data music = test_data_generator::generate_random_music(500, 5000);

    // Assignment should be fast
    engine.assign_channels(music);
}

REGISTER_PERFORMANCE_TEST(channel_assignment, strategy_switching) {
    static channel_assignment_engine engine;
    static music_data music = test_data_generator::generate_random_music(100, 2000);

    std::vector<std::string> strategies = {"authentic_nes", "modern_chiptune", "quality_focused"};

    for (const auto& strategy : strategies) {
        engine.set_active_strategy(strategy);
        engine.assign_channels(music);
    }
}

// Stress tests for channel assignment
REGISTER_STRESS_TEST(channel_assignment, concurrent_analysis) {
    static channel_assignment_engine engine;
    static music_data music = test_data_generator::generate_random_music(200, 3000);

    // Multiple threads analyzing the same music simultaneously
    engine.analyze_all_tracks(music);
}

REGISTER_STRESS_TEST(channel_assignment, concurrent_assignment) {
    static channel_assignment_engine engine;
    static music_data music = test_data_generator::generate_random_music(150, 2500);

    // Multiple threads performing assignment simultaneously
    engine.assign_channels(music);
}

REGISTER_STRESS_TEST(channel_assignment, memory_usage) {
    channel_assignment_engine engine;

    // Create and process many different music pieces
    for (int i = 0; i < 10; ++i) {
        music_data music = test_data_generator::generate_random_music(100 + i * 50, 2000);
        assignment_result result = engine.assign_channels(music);

        // Verify results are reasonable
        ASSERT_LE(result.assignments.size(), 5);
        ASSERT_GE(result.overall_quality_score, 0.0f);
        ASSERT_LE(result.overall_quality_score, 1.0f);
    }
}

// Integration tests with different musical styles
REGISTER_TEST(channel_assignment, classical_music_style) {
    channel_assignment_engine engine;
    engine.set_active_strategy("quality_focused");

    // Simulate classical music arrangement
    music_data music;

    // Melody line
    for (int i = 0; i < 16; ++i) {
        music.add_note(music_note(0, 60 + (i % 8), 80 + (i % 40), i * 240, 240));
    }

    // Harmony
    for (int i = 0; i < 8; ++i) {
        music.add_note(music_note(1, 52 + (i % 6), 70, i * 480, 480));
    }

    // Bass line
    for (int i = 0; i < 8; ++i) {
        music.add_note(music_note(2, 36 + (i % 4), 90, i * 480, 480));
    }

    assignment_result result = engine.assign_channels(music);

    ASSERT_FALSE(result.assignments.empty());
    ASSERT_GT(result.overall_quality_score, 0.6f);  // Should be good quality
}

REGISTER_TEST(channel_assignment, electronic_music_style) {
    channel_assignment_engine engine;
    engine.set_active_strategy("modern_chiptune");

    // Simulate electronic/chiptune music
    music_data music;

    // Fast arpeggios
    for (int i = 0; i < 32; ++i) {
        music.add_note(music_note(0, 60 + (i % 12), 100, i * 60, 60));
    }

    // Pulsing bass
    for (int i = 0; i < 16; ++i) {
        music.add_note(music_note(2, 36, 127, i * 120, 60));
    }

    // Electronic drums
    for (int i = 0; i < 16; ++i) {
        music.add_note(music_note(9, 36, 127, i * 120, 30));  // Kick
        if (i % 2 == 1) {
            music.add_note(music_note(9, 38, 100, i * 120 + 60, 30));  // Snare
        }
    }

    assignment_result result = engine.assign_channels(music);

    ASSERT_FALSE(result.assignments.empty());
    ASSERT_GT(result.overall_quality_score, 0.7f);  // Should be high quality for chiptune
}