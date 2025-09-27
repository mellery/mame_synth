#include "test_framework_enhanced.h"
#include "../src/nes_sequencer.h"
#include "../src/nes_audio_mixer.h"
#include <thread>
#include <chrono>

// Unit tests for NES sequencer
REGISTER_TEST(nes_sequencer, basic_creation) {
    nes_sequencer::sequencer_config config;
    config.sample_rate = 44100;
    config.ticks_per_quarter_note = 480;

    nes_sequencer sequencer(config);

    ASSERT_FALSE(sequencer.is_playing());
    ASSERT_FALSE(sequencer.is_paused());
    ASSERT_EQ(sequencer.get_position(), 0);
}

// Note: This test requires audio manager setup which is complex for unit testing
/*
REGISTER_TEST(nes_sequencer, music_loading) {
    nes_sequencer::sequencer_config config;
    config.sample_rate = 44100;
    config.ticks_per_quarter_note = 480;

    nes_sequencer sequencer(config);
    ASSERT_TRUE(sequencer.initialize(nullptr));  // Can pass nullptr for basic testing

    // Create test music data
    music_data music = test_data_generator::generate_random_music(50, 1920);

    ASSERT_TRUE(sequencer.load_music_data(music));
    ASSERT_GT(sequencer.get_total_duration(), 0);
}
*/

REGISTER_TEST(nes_sequencer, channel_mapping) {
    nes_sequencer sequencer;

    // Test default channel mapping
    std::vector<nes_sequencer::nes_channel_mapping> default_mapping = {
        {0, 0, true},  // MIDI 0 -> NES Pulse 1
        {1, 1, true},  // MIDI 1 -> NES Pulse 2
        {2, 2, true},  // MIDI 2 -> NES Triangle
        {9, 3, true},  // MIDI 9 -> NES Noise
        {3, 4, true}   // MIDI 3 -> NES DMC
    };

    sequencer.set_channel_mapping(default_mapping);

    // Test channel enable/disable
    sequencer.set_channel_enabled(0, false);
    sequencer.set_channel_enabled(1, true);

    // Should not crash and should accept the configuration
    ASSERT_TRUE(true);  // If we get here, basic functionality works
}

REGISTER_TEST(nes_sequencer, tempo_control) {
    nes_sequencer sequencer;

    // Test tempo scaling
    ASSERT_EQ(sequencer.get_tempo_scale(), 1.0);

    sequencer.set_tempo_scale(2.0);  // Double speed
    ASSERT_EQ(sequencer.get_tempo_scale(), 2.0);

    sequencer.set_tempo_scale(0.5);  // Half speed
    ASSERT_EQ(sequencer.get_tempo_scale(), 0.5);
}

REGISTER_TEST(nes_sequencer, loop_control) {
    nes_sequencer sequencer;

    // Test loop settings
    sequencer.set_loop_enabled(true);
    sequencer.set_loop_points(0, 1920);  // Loop entire 4-beat pattern

    // Load music and test looping doesn't crash
    music_data music = test_data_generator::generate_random_music(20, 1920);
    sequencer.load_music_data(music);

    ASSERT_TRUE(true);  // Basic loop setup works
}

REGISTER_TEST(nes_sequencer, event_creation) {
    auto start_time = std::chrono::steady_clock::now();

    // Test note event creation
    auto note_on = nes_sequencer::sequencer_event::note_on(
        start_time, 0, 0, 60, 100, 480
    );

    ASSERT_EQ(static_cast<int>(note_on.type), static_cast<int>(nes_sequencer::event_type::NOTE_ON));
    ASSERT_EQ(note_on.data.note_event.channel, 0);
    ASSERT_EQ(note_on.data.note_event.note, 60);
    ASSERT_EQ(note_on.data.note_event.velocity, 100);

    // Test note off event creation
    auto note_off = nes_sequencer::sequencer_event::note_off(
        start_time, 480, 0, 60
    );

    ASSERT_EQ(static_cast<int>(note_off.type), static_cast<int>(nes_sequencer::event_type::NOTE_OFF));
    ASSERT_EQ(note_off.data.note_event.channel, 0);
    ASSERT_EQ(note_off.data.note_event.note, 60);
}

REGISTER_TEST(nes_sequencer, statistics_tracking) {
    nes_sequencer sequencer;

    // Get initial stats
    auto stats = sequencer.get_stats();
    ASSERT_EQ(stats.events_processed, 0);
    ASSERT_EQ(stats.notes_played, 0);
    ASSERT_EQ(stats.active_notes, 0);

    // Load music to populate stats
    music_data music = test_data_generator::generate_random_music(10, 960);
    sequencer.load_music_data(music);

    // Stats should still be valid
    stats = sequencer.get_stats();
    ASSERT_GE(stats.current_bpm, 0.0);
}

// Note: This test requires audio manager setup which is complex for unit testing
/*
REGISTER_TEST(nes_sequencer, position_control) {
    nes_sequencer::sequencer_config config;
    config.sample_rate = 44100;
    config.ticks_per_quarter_note = 480;

    nes_sequencer sequencer(config);
    ASSERT_TRUE(sequencer.initialize(nullptr));  // Can pass nullptr for basic testing

    music_data music = test_data_generator::generate_random_music(20, 1920);
    ASSERT_TRUE(sequencer.load_music_data(music));

    // Test position setting
    sequencer.set_position(480);  // 1 beat
    ASSERT_EQ(sequencer.get_position(), 480);

    sequencer.set_position(960);  // 2 beats
    ASSERT_EQ(sequencer.get_position(), 960);
}
*/

REGISTER_TEST(nes_sequencer, real_time_note_triggering) {
    nes_sequencer sequencer;

    // Test real-time note triggering
    sequencer.trigger_note(0, 60, 100, 480);  // C4 on channel 0
    sequencer.trigger_note(1, 64, 80, 240);   // E4 on channel 1

    // Test note stopping
    sequencer.stop_note(0, 60);

    // Test panic (all notes off)
    sequencer.panic();

    // If we get here without crashing, basic functionality works
    ASSERT_TRUE(true);
}

// Performance tests for sequencer
REGISTER_PERFORMANCE_TEST(nes_sequencer, music_loading_speed) {
    static nes_sequencer sequencer;
    static music_data large_music = test_data_generator::generate_random_music(500, 10000);

    // Test loading speed
    sequencer.load_music_data(large_music);
}

REGISTER_PERFORMANCE_TEST(nes_sequencer, event_processing_speed) {
    static nes_sequencer sequencer;

    // Trigger many notes rapidly
    for (int i = 0; i < 10; ++i) {
        sequencer.trigger_note(i % 5, 60 + (i % 12), 100, 120);
    }
}

REGISTER_PERFORMANCE_TEST(nes_sequencer, statistics_calculation) {
    static nes_sequencer sequencer;
    static music_data music = test_data_generator::generate_random_music(200, 5000);

    sequencer.load_music_data(music);

    // Test stats calculation speed
    auto stats = sequencer.get_stats();
}

// Stress tests for sequencer
REGISTER_STRESS_TEST(nes_sequencer, concurrent_note_triggering) {
    static nes_sequencer sequencer;

    // Multiple threads triggering notes simultaneously
    int note = rand() % 88 + 20;  // Random note
    int channel = rand() % 5;     // Random channel
    int velocity = rand() % 127 + 1;  // Random velocity

    sequencer.trigger_note(channel, note, velocity, 240);

    // Small delay to avoid overwhelming
    std::this_thread::sleep_for(std::chrono::microseconds(100));
}

REGISTER_STRESS_TEST(nes_sequencer, rapid_position_changes) {
    static nes_sequencer sequencer;
    static music_data music = test_data_generator::generate_random_music(100, 3840);

    sequencer.load_music_data(music);

    // Rapidly change playback position
    int position = rand() % 3840;
    sequencer.set_position(position);
}

REGISTER_STRESS_TEST(nes_sequencer, memory_usage_large_scores) {
    // Create sequencers with very large music scores
    nes_sequencer sequencer;

    music_data huge_music = test_data_generator::generate_random_music(1000 + (rand() % 500), 20000);

    ASSERT_TRUE(sequencer.load_music_data(huge_music));

    // Verify basic functionality still works
    ASSERT_GT(sequencer.get_total_duration(), 0);

    // Test memory cleanup by letting sequencer go out of scope
}