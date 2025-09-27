#include "test_framework.h"
#include "nes_playback_engine.h"
#include "nes_cli.h"
#include "nes_config.h"
#include "nes_realtime_control.h"
#include "music_parser.h"
#include <fstream>
#include <chrono>
#include <thread>
#include <memory>

/**
 * Comprehensive NES Integration Tests
 *
 * Tests the complete workflow from MIDI input through NES playback to audio output,
 * including configuration, real-time control, and CLI integration.
 */

// Test helper to create a simple MIDI file
std::vector<uint8_t> create_test_midi_file() {
    std::vector<uint8_t> midi_data;

    // MIDI header "MThd"
    midi_data.insert(midi_data.end(), {'M', 'T', 'h', 'd'});
    midi_data.insert(midi_data.end(), {0x00, 0x00, 0x00, 0x06}); // Header length
    midi_data.insert(midi_data.end(), {0x00, 0x00}); // Format 0
    midi_data.insert(midi_data.end(), {0x00, 0x01}); // 1 track
    midi_data.insert(midi_data.end(), {0x01, 0xE0}); // 480 ticks per quarter

    // Track header "MTrk"
    midi_data.insert(midi_data.end(), {'M', 'T', 'r', 'k'});

    // Track data: Simple NES-style melody
    std::vector<uint8_t> track_data = {
        0x00, 0x90, 0x3C, 0x64, // Note on C4 (square wave 1)
        0x10, 0x91, 0x40, 0x50, // Note on E4 (square wave 2)
        0x10, 0x92, 0x43, 0x40, // Note on G4 (triangle)
        0x20, 0x80, 0x3C, 0x40, // Note off C4
        0x00, 0x81, 0x40, 0x40, // Note off E4
        0x00, 0x82, 0x43, 0x40, // Note off G4
        0x00, 0xFF, 0x2F, 0x00  // End of track
    };

    // Track length
    uint32_t track_length = track_data.size();
    midi_data.push_back((track_length >> 24) & 0xFF);
    midi_data.push_back((track_length >> 16) & 0xFF);
    midi_data.push_back((track_length >> 8) & 0xFF);
    midi_data.push_back(track_length & 0xFF);

    midi_data.insert(midi_data.end(), track_data.begin(), track_data.end());

    return midi_data;
}

// Test helper to write MIDI data to file
void write_test_midi_file(const std::string& filename, const std::vector<uint8_t>& data) {
    std::ofstream file(filename, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();
}

REGISTER_TEST(nes_integration, complete_midi_to_audio_workflow) {
    // Create test MIDI file
    auto midi_data = create_test_midi_file();
    write_test_midi_file("test_nes_integration.mid", midi_data);

    // Create NES playback engine with file output
    nes_playback_engine::engine_config config;
    config.sample_rate = 44100;
    config.buffer_size = 1024;
    config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;

    auto engine = std::make_unique<nes_playback_engine>(config);
    ASSERT_TRUE(engine->initialize());

    // Load and parse MIDI file
    midi_parser parser;
    music_data music;
    ASSERT_TRUE(parser.parse_file("test_nes_integration.mid", music));

    // Load music into engine
    ASSERT_TRUE(engine->load_music_data(music));

    // Export to WAV instead of real-time playback
    std::string test_filename = "test_nes_output.wav";
    ASSERT_TRUE(engine->export_to_wav(test_filename, 44100));

    // Verify engine state
    ASSERT_FALSE(engine->is_playing());

    // Check that output file was created
    std::ifstream output_file(test_filename);
    ASSERT_TRUE(output_file.good());
    output_file.close();

    // Cleanup
    engine->shutdown();
}

REGISTER_TEST(nes_integration, configuration_system_workflow) {
    // Test the complete configuration workflow

    // Create default configuration
    nes_config::nes_configuration config; // Use default constructor

    // Modify some settings
    config.audio.sample_rate = 48000;
    config.audio.enable_nonlinear_mixing = true;
    config.performance.enable_multithreading = false;
    config.ui.verbose_output = true;

    // Save configuration to file
    ASSERT_TRUE(nes_config::nes_config_manager::save_configuration("test_config.json", config));

    // Load configuration back
    nes_config::nes_configuration loaded_config;
    ASSERT_TRUE(nes_config::nes_config_manager::load_configuration("test_config.json", loaded_config));

    // Verify settings were preserved
    ASSERT_EQ(48000, loaded_config.audio.sample_rate);
    ASSERT_TRUE(loaded_config.audio.enable_nonlinear_mixing);
    ASSERT_FALSE(loaded_config.performance.enable_multithreading);
    ASSERT_TRUE(loaded_config.ui.verbose_output);

    // Test preset configurations
    auto performance_config = nes_config::nes_config_manager::create_performance_preset();
    ASSERT_FALSE(performance_config.audio.enable_nonlinear_mixing); // Should be disabled for performance

    auto quality_config = nes_config::nes_config_manager::create_quality_preset();
    ASSERT_TRUE(quality_config.audio.enable_nonlinear_mixing); // Should be enabled for quality
}

REGISTER_TEST(nes_integration, realtime_control_workflow) {
    // Test real-time parameter control integration

    // Create engine with minimal configuration
    nes_playback_engine::engine_config config;
    config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;

    auto engine = std::make_unique<nes_playback_engine>(config);
    ASSERT_TRUE(engine->initialize());

    // Create real-time controller
    auto realtime_controller = std::make_unique<nes_realtime::realtime_parameter_controller>();
    realtime_controller->set_playback_engine(engine.get());

    // Test parameter registration
    nes_realtime::parameter_value volume_param(1.0f, 0.0f, 1.0f, "Master volume", "");
    ASSERT_TRUE(realtime_controller->register_parameter(
        nes_realtime::parameter_type::MASTER_VOLUME, 0, volume_param));

    // Test parameter setting
    ASSERT_TRUE(realtime_controller->set_parameter(
        nes_realtime::parameter_type::MASTER_VOLUME, 0, 0.5f, "test"));

    // Test parameter getting
    auto retrieved_param = realtime_controller->get_parameter(
        nes_realtime::parameter_type::MASTER_VOLUME, 0);
    ASSERT_NEAR(0.5f, retrieved_param.value, 0.01f);

    // Test MIDI mapping
    nes_realtime::midi_mapping mapping(7, nes_realtime::parameter_type::MASTER_VOLUME, 0);
    realtime_controller->add_midi_mapping(mapping);

    // Test preset functionality
    realtime_controller->save_preset("test_preset");
    auto presets = realtime_controller->get_preset_names();
    bool found_preset = false;
    for (const auto& preset : presets) {
        if (preset == "test_preset") {
            found_preset = true;
            break;
        }
    }
    ASSERT_TRUE(found_preset);

    // Cleanup
    engine->shutdown();
}

REGISTER_TEST(nes_integration, cli_command_workflow) {
    // Test CLI command integration

    nes_cli::cli_config cli_config;
    cli_config.audio_backend = "file";
    cli_config.output_file = "test_cli_output.wav";
    cli_config.quiet = true; // Reduce output during testing

    nes_cli cli(cli_config);

    // Test basic commands
    auto result = cli.run_command("help");
    ASSERT_TRUE(result.success);

    result = cli.run_command("version");
    ASSERT_TRUE(result.success);

    // Test configuration commands
    result = cli.run_command("config-preset performance");
    ASSERT_TRUE(result.success);

    result = cli.run_command("config-show audio");
    ASSERT_TRUE(result.success);

    // Test real-time control commands (these should work even without audio playback)
    result = cli.run_command("realtime-stats");
    ASSERT_TRUE(result.success);

    result = cli.run_command("cv 0 0.8");
    ASSERT_TRUE(result.success);

    result = cli.run_command("cp 1 -0.3");
    ASSERT_TRUE(result.success);

    result = cli.run_command("pd 0 2");
    ASSERT_TRUE(result.success);
}

REGISTER_TEST(nes_integration, multi_channel_playback) {
    // Test multi-channel NES playback with all 5 channels

    // Create a more complex MIDI file that uses multiple channels
    std::vector<uint8_t> midi_data;

    // MIDI header
    midi_data.insert(midi_data.end(), {'M', 'T', 'h', 'd'});
    midi_data.insert(midi_data.end(), {0x00, 0x00, 0x00, 0x06});
    midi_data.insert(midi_data.end(), {0x00, 0x01, 0x00, 0x01, 0x01, 0xE0});

    // Track header
    midi_data.insert(midi_data.end(), {'M', 'T', 'r', 'k'});

    // Track data with all 5 NES channels
    std::vector<uint8_t> track_data = {
        // Channel 0 (Pulse 1) - C4
        0x00, 0x90, 0x3C, 0x64,
        // Channel 1 (Pulse 2) - E4
        0x00, 0x91, 0x40, 0x64,
        // Channel 2 (Triangle) - G4
        0x00, 0x92, 0x43, 0x64,
        // Channel 9 (Noise) - Percussion
        0x00, 0x99, 0x24, 0x64,
        // Channel 3 (DMC) - Low note
        0x00, 0x93, 0x30, 0x64,

        // Wait and turn off all notes
        0x60, // Delta time
        0x80, 0x3C, 0x40, // Pulse 1 off
        0x00, 0x81, 0x40, 0x40, // Pulse 2 off
        0x00, 0x82, 0x43, 0x40, // Triangle off
        0x00, 0x89, 0x24, 0x40, // Noise off
        0x00, 0x83, 0x30, 0x40, // DMC off

        0x00, 0xFF, 0x2F, 0x00  // End of track
    };

    // Track length
    uint32_t track_length = track_data.size();
    midi_data.push_back((track_length >> 24) & 0xFF);
    midi_data.push_back((track_length >> 16) & 0xFF);
    midi_data.push_back((track_length >> 8) & 0xFF);
    midi_data.push_back(track_length & 0xFF);

    midi_data.insert(midi_data.end(), track_data.begin(), track_data.end());

    // Write test file
    write_test_midi_file("test_multichannel.mid", midi_data);

    // Create engine
    nes_playback_engine::engine_config config;
    config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;

    auto engine = std::make_unique<nes_playback_engine>(config);
    ASSERT_TRUE(engine->initialize());

    // Load and play
    midi_parser parser;
    music_data music;
    ASSERT_TRUE(parser.parse_file("test_multichannel.mid", music));
    ASSERT_TRUE(engine->load_music_data(music));

    // Export to WAV instead of real-time playback
    std::string test_filename = "test_multichannel_output.wav";
    ASSERT_TRUE(engine->export_to_wav(test_filename, 44100));

    ASSERT_FALSE(engine->is_playing());

    // Verify output file
    std::ifstream output_file(test_filename);
    ASSERT_TRUE(output_file.good());
    output_file.close();

    engine->shutdown();
}

REGISTER_TEST(nes_integration, error_handling_workflow) {
    // Test error handling in various failure scenarios

    // Test invalid MIDI file
    nes_playback_engine::engine_config config;
    config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;

    auto engine = std::make_unique<nes_playback_engine>(config);
    ASSERT_TRUE(engine->initialize());

    midi_parser parser;
    music_data music;

    // Try to parse non-existent file
    ASSERT_FALSE(parser.parse_file("nonexistent.mid", music));

    // Create invalid MIDI file
    std::vector<uint8_t> invalid_data = {0x00, 0x01, 0x02, 0x03}; // Not valid MIDI
    write_test_midi_file("invalid.mid", invalid_data);
    ASSERT_FALSE(parser.parse_file("invalid.mid", music));

    // Test engine operations without loaded music
    // Test should fail when trying to export without music loaded
    std::string test_filename = "test_error_output.wav";
    ASSERT_FALSE(engine->export_to_wav(test_filename, 44100)); // Should fail - no music loaded

    engine->shutdown();
}

REGISTER_TEST(nes_integration, performance_monitoring) {
    // Test performance monitoring and statistics

    nes_playback_engine::engine_config config;
    config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;
    config.enable_performance_monitoring = true;

    auto engine = std::make_unique<nes_playback_engine>(config);
    ASSERT_TRUE(engine->initialize());

    // Create and load test music
    auto midi_data = create_test_midi_file();
    write_test_midi_file("test_performance.mid", midi_data);

    midi_parser parser;
    music_data music;
    ASSERT_TRUE(parser.parse_file("test_performance.mid", music));
    ASSERT_TRUE(engine->load_music_data(music));

    // Export to WAV and monitor performance
    std::string test_filename = "test_performance_output.wav";
    ASSERT_TRUE(engine->export_to_wav(test_filename, 44100));

    // Get performance stats
    auto stats = engine->get_performance_metrics();
    ASSERT_GT(stats.total_playback_time_ms, 0);
    engine->shutdown();
}

REGISTER_TEST(nes_integration, configuration_integration_with_engine) {
    // Test that configuration changes properly affect engine behavior

    // Create configuration with specific settings
    nes_config::nes_configuration config;
    config.audio.sample_rate = 22050; // Lower sample rate
    config.audio.buffer_size = 512;   // Smaller buffer
    config.audio.enable_nonlinear_mixing = false; // Disable for simpler output

    // Apply configuration to engine config
    nes_playback_engine::engine_config engine_config;
    // TODO: Add apply_to_engine_config method to nes_config_manager
    engine_config.sample_rate = config.audio.sample_rate;
    engine_config.buffer_size = config.audio.buffer_size;
    engine_config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;

    auto engine = std::make_unique<nes_playback_engine>(engine_config);
    ASSERT_TRUE(engine->initialize());

    // Verify configuration was applied
    // TODO: Add getter methods to verify engine configuration

    engine->shutdown();
}