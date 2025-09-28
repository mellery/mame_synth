#include "test_framework.h"
#include <memory>
#include <chrono>
#include <thread>
#include "nes_playback_engine.h"
#include "audio_device.h"
#include "nes_audio_mixer.h"

// Helper function to create test engine
static std::unique_ptr<nes_playback_engine> create_test_engine() {
    nes_playback_engine::engine_config config{};
    config.sample_rate = 44100;
    config.buffer_size = 512;
    config.enable_performance_monitoring = true;
    config.enable_looping = false;
    return std::make_unique<nes_playback_engine>(config);
}

// Test engine initialization and configuration
REGISTER_TEST(nes_playback_engine, basic_initialization) {
    auto engine = create_test_engine();

    ASSERT_FALSE(engine->is_ready());
    ASSERT_TRUE(engine->initialize());
    ASSERT_TRUE(engine->is_ready());

    // Check initial state
    ASSERT_TRUE(engine->get_state() == nes_playback_engine::engine_state::INITIALIZED);
    ASSERT_FALSE(engine->is_playing());

    // Verify config was applied
    auto retrieved_config = engine->get_config();
    ASSERT_EQ(retrieved_config.sample_rate, 44100);
    ASSERT_EQ(retrieved_config.buffer_size, 512);

    engine->shutdown();
}

REGISTER_TEST(nes_playback_engine, configuration_management) {
    auto engine = create_test_engine();
    ASSERT_TRUE(engine->initialize());

    // Test config modification
    auto new_config = engine->get_config();
    new_config.sample_rate = 22050;
    new_config.buffer_size = 256;
    new_config.enable_looping = true;

    engine->set_config(new_config);
    auto retrieved = engine->get_config();

    ASSERT_EQ(retrieved.sample_rate, 22050);
    ASSERT_EQ(retrieved.buffer_size, 256);
    ASSERT_TRUE(retrieved.enable_looping);

    engine->shutdown();
}

// Test channel volume control
REGISTER_TEST(nes_playback_engine, channel_volume_control) {
    auto engine = create_test_engine();
    ASSERT_TRUE(engine->initialize());

    // Test setting volume on all NES channels (0-4)
    for (uint8_t channel = 0; channel < 5; ++channel) {
        // Test valid volume range
        engine->set_channel_volume(channel, 0.0f);  // Min volume
        engine->set_channel_volume(channel, 0.5f);  // Mid volume
        engine->set_channel_volume(channel, 1.0f);  // Max volume

        // Test out-of-range values (should be clamped)
        engine->set_channel_volume(channel, -0.5f); // Below min
        engine->set_channel_volume(channel, 1.5f);  // Above max
    }

    // Test invalid channel (should not crash)
    engine->set_channel_volume(255, 0.5f);

    engine->shutdown();
}

// Test channel muting functionality
REGISTER_TEST(nes_playback_engine, channel_muting) {
    auto engine = create_test_engine();
    ASSERT_TRUE(engine->initialize());

    // Test muting and unmuting all channels
    for (uint8_t channel = 0; channel < 5; ++channel) {
        // Mute channel
        engine->mute_channel(channel, true);

        // Unmute channel
        engine->mute_channel(channel, false);
    }

    // Test with invalid channel
    engine->mute_channel(255, true);

    engine->shutdown();
}

// Test pulse duty cycle control
REGISTER_TEST(nes_playback_engine, pulse_duty_cycle_control) {
    auto engine = create_test_engine();
    ASSERT_TRUE(engine->initialize());

    // Test on valid pulse channels (0, 1)
    for (uint8_t channel = 0; channel < 2; ++channel) {
        // Test all valid duty cycle values (0-3)
        for (uint8_t duty = 0; duty < 4; ++duty) {
            engine->set_pulse_duty_cycle(channel, duty);
        }

        // Test out-of-range duty cycle (should be clamped)
        engine->set_pulse_duty_cycle(channel, 255);
    }

    // Test on non-pulse channels (should be ignored safely)
    engine->set_pulse_duty_cycle(2, 1); // Triangle channel
    engine->set_pulse_duty_cycle(3, 1); // Noise channel
    engine->set_pulse_duty_cycle(4, 1); // DMC channel

    engine->shutdown();
}

// Test triangle linear counter
REGISTER_TEST(nes_playback_engine, triangle_linear_counter) {
    auto engine = create_test_engine();
    ASSERT_TRUE(engine->initialize());

    // Test valid range (0-127)
    engine->set_triangle_linear_counter(0);
    engine->set_triangle_linear_counter(64);
    engine->set_triangle_linear_counter(127);

    // Test out-of-range value (should be clamped)
    engine->set_triangle_linear_counter(255);

    engine->shutdown();
}

// Test noise mode control
REGISTER_TEST(nes_playback_engine, noise_mode_control) {
    auto engine = create_test_engine();
    ASSERT_TRUE(engine->initialize());

    // Test both noise modes
    engine->set_noise_mode(true);   // Short mode (93-bit)
    engine->set_noise_mode(false);  // Long mode (32767-bit)

    engine->shutdown();
}

// Test loop control functionality
REGISTER_TEST(nes_playback_engine, loop_control) {
    auto engine = create_test_engine();
    ASSERT_TRUE(engine->initialize());

    // Test initial loop state
    ASSERT_FALSE(engine->is_loop_enabled()); // Should match config default

    // Enable looping
    engine->set_loop_enabled(true);
    ASSERT_TRUE(engine->is_loop_enabled());

    // Disable looping
    engine->set_loop_enabled(false);
    ASSERT_FALSE(engine->is_loop_enabled());

    engine->shutdown();

    // Test with uninitialized engine
    auto config = nes_playback_engine::engine_config{};
    auto uninitialized_engine = std::make_unique<nes_playback_engine>(config);
    uninitialized_engine->set_loop_enabled(true);
    // Should not crash and should return config value
    ASSERT_EQ(uninitialized_engine->is_loop_enabled(), config.enable_looping);
}

// Test engine state management
REGISTER_TEST(nes_playback_engine, state_management) {
    auto engine = create_test_engine();

    // Test uninitialized state
    ASSERT_TRUE(engine->get_state() == nes_playback_engine::engine_state::UNINITIALIZED);
    ASSERT_FALSE(engine->is_playing());
    ASSERT_FALSE(engine->is_ready());

    // Test initialization
    ASSERT_TRUE(engine->initialize());
    ASSERT_TRUE(engine->get_state() == nes_playback_engine::engine_state::INITIALIZED);
    ASSERT_TRUE(engine->is_ready());

    // Test reset
    ASSERT_TRUE(engine->reset());
    ASSERT_TRUE(engine->get_state() == nes_playback_engine::engine_state::READY);

    // Test shutdown
    ASSERT_TRUE(engine->shutdown());
}

// Test performance metrics
REGISTER_TEST(nes_playback_engine, performance_metrics) {
    auto engine = create_test_engine();
    ASSERT_TRUE(engine->initialize());

    // Get initial metrics
    auto metrics = engine->get_performance_metrics();
    ASSERT_EQ(metrics.files_loaded, 0);
    ASSERT_EQ(metrics.playback_sessions, 0);

    // Reset metrics
    engine->reset_performance_metrics();
    auto reset_metrics = engine->get_performance_metrics();
    ASSERT_EQ(reset_metrics.files_loaded, 0);
    ASSERT_EQ(reset_metrics.playback_sessions, 0);

    engine->shutdown();
}

// Test master volume control
REGISTER_TEST(nes_playback_engine, master_volume_control) {
    auto engine = create_test_engine();
    ASSERT_TRUE(engine->initialize());

    // Test setting master volume
    engine->set_master_volume(0.0f);  // Min
    engine->set_master_volume(0.5f);  // Mid
    engine->set_master_volume(1.0f);  // Max

    // Test out-of-range values
    engine->set_master_volume(-0.5f); // Below min
    engine->set_master_volume(2.0f);  // Above max

    engine->shutdown();
}

// Test error handling with uninitialized engine
REGISTER_TEST(nes_playback_engine, error_handling_uninitialized) {
    auto config = nes_playback_engine::engine_config{};
    auto engine = std::make_unique<nes_playback_engine>(config);

    // Don't initialize the engine, test methods handle this gracefully

    // These should not crash with uninitialized engine
    engine->set_channel_volume(0, 0.5f);
    engine->mute_channel(0, true);
    engine->set_pulse_duty_cycle(0, 1);
    engine->set_triangle_linear_counter(64);
    engine->set_noise_mode(true);
    engine->set_master_volume(0.5f);

    // Loop control should work even without full initialization
    engine->set_loop_enabled(true);
    ASSERT_EQ(engine->is_loop_enabled(), config.enable_looping);
}

// Test boundary conditions
REGISTER_TEST(nes_playback_engine, boundary_conditions) {
    auto engine = create_test_engine();
    ASSERT_TRUE(engine->initialize());

    // Test with extreme channel numbers
    engine->set_channel_volume(0, 0.5f);    // Valid min channel
    engine->set_channel_volume(4, 0.5f);    // Valid max channel
    engine->set_channel_volume(5, 0.5f);    // Invalid channel
    engine->set_channel_volume(255, 0.5f);  // Max uint8_t

    // Test pulse channels boundary
    engine->set_pulse_duty_cycle(0, 0);     // Valid min
    engine->set_pulse_duty_cycle(1, 3);     // Valid max
    engine->set_pulse_duty_cycle(2, 1);     // Invalid channel

    // Test triangle counter boundary
    engine->set_triangle_linear_counter(0);   // Min valid
    engine->set_triangle_linear_counter(127); // Max valid
    engine->set_triangle_linear_counter(128); // Should be clamped

    engine->shutdown();
}

// Test factory configurations
REGISTER_TEST(nes_playback_engine, factory_configurations) {
    // Test implemented preset configurations
    auto high_quality = nes_playback_engine_factory::high_quality_config();
    auto low_latency = nes_playback_engine_factory::low_latency_config();

    // Verify configurations are different and valid
    ASSERT_TRUE(high_quality.sample_rate != low_latency.sample_rate);
    ASSERT_TRUE(low_latency.buffer_size != high_quality.buffer_size);

    // Verify specific settings
    ASSERT_EQ(high_quality.sample_rate, 48000);
    ASSERT_EQ(high_quality.buffer_size, 2048);
    ASSERT_EQ(low_latency.sample_rate, 44100);
    ASSERT_EQ(low_latency.buffer_size, 256);

    // Test implemented factory creation methods
    auto music_engine = nes_playback_engine_factory::create_for_music_playback();
    auto interactive_engine = nes_playback_engine_factory::create_for_interactive_use();

    ASSERT_TRUE(music_engine != nullptr);
    ASSERT_TRUE(interactive_engine != nullptr);

    // Test these engines can initialize
    ASSERT_TRUE(music_engine->initialize());
    ASSERT_TRUE(interactive_engine->initialize());

    // Clean up
    music_engine->shutdown();
    interactive_engine->shutdown();
}