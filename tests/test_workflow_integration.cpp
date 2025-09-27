#include "test_framework.h"
#include "nes_playback_engine.h"
#include "nes_cli.h"
#include "nes_config.h"
#include "nes_realtime_control.h"
#include "music_parser.h"
#include <fstream>
#include <thread>
#include <chrono>

/**
 * Complete Workflow Integration Tests
 *
 * Tests end-to-end workflows that combine multiple components:
 * configuration + playback + real-time control + CLI interaction
 */

REGISTER_TEST(workflow_integration, config_to_playback_workflow) {
    // Test complete workflow: Load config -> Create engine -> Play music

    // Create a custom configuration
    nes_config::nes_configuration config = nes_config::nes_config_manager::create_quality_preset();
    config.audio.sample_rate = 22050; // Lower for faster testing
    config.audio.buffer_size = 512;
    config.audio.enable_nonlinear_mixing = true;

    // Save configuration
    ASSERT_TRUE(nes_config::nes_config_manager::save_configuration("workflow_config.json", config));

    // Create engine from configuration
    nes_playback_engine::engine_config engine_config;
    engine_config.sample_rate = config.audio.sample_rate;
    engine_config.buffer_size = config.audio.buffer_size;
    engine_config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;

    auto engine = std::make_unique<nes_playback_engine>(engine_config);
    ASSERT_TRUE(engine->initialize());

    // Create simple test music
    std::vector<uint8_t> midi_data;
    midi_data.insert(midi_data.end(), {'M', 'T', 'h', 'd'});
    midi_data.insert(midi_data.end(), {0x00, 0x00, 0x00, 0x06});
    midi_data.insert(midi_data.end(), {0x00, 0x00, 0x00, 0x01, 0x01, 0xE0});
    midi_data.insert(midi_data.end(), {'M', 'T', 'r', 'k'});

    std::vector<uint8_t> track_data = {
        0x00, 0x90, 0x3C, 0x64, // Note on C4
        0x60, 0x80, 0x3C, 0x40, // Note off C4 after 96 ticks
        0x00, 0xFF, 0x2F, 0x00  // End of track
    };

    uint32_t track_length = track_data.size();
    midi_data.push_back((track_length >> 24) & 0xFF);
    midi_data.push_back((track_length >> 16) & 0xFF);
    midi_data.push_back((track_length >> 8) & 0xFF);
    midi_data.push_back(track_length & 0xFF);
    midi_data.insert(midi_data.end(), track_data.begin(), track_data.end());

    // Write MIDI file
    std::ofstream midi_file("workflow_test.mid", std::ios::binary);
    midi_file.write(reinterpret_cast<const char*>(midi_data.data()), midi_data.size());
    midi_file.close();

    // Load and play music
    midi_parser parser;
    music_data music;
    ASSERT_TRUE(parser.parse_file("workflow_test.mid", music));
    ASSERT_TRUE(engine->load_music_data(music));
    ASSERT_TRUE(engine->play());

    // Let it play briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Stop and verify
    engine->stop();
    ASSERT_FALSE(engine->is_playing());

    // Verify output file was created
    std::ifstream output_file("workflow_output.wav");
    ASSERT_TRUE(output_file.good());
    output_file.close();

    engine->shutdown();
}

REGISTER_TEST(workflow_integration, cli_to_realtime_control_workflow) {
    // Test workflow: CLI config changes -> Real-time parameter control

    nes_cli::cli_config cli_config;
    cli_config.quiet = true;
    cli_config.audio_backend = "file";
    cli_config.output_file = "cli_realtime_output.wav";

    nes_cli cli(cli_config);

    // Configure via CLI
    auto result = cli.run_command("config-preset performance");
    ASSERT_TRUE(result.success);

    result = cli.run_command("config-set audio.sample_rate 22050");
    ASSERT_TRUE(result.success);

    // Set real-time parameters via CLI
    result = cli.run_command("cv 0 0.8");   // Channel 0 volume
    ASSERT_TRUE(result.success);

    result = cli.run_command("cv 1 0.6");   // Channel 1 volume
    ASSERT_TRUE(result.success);

    result = cli.run_command("cp 0 -0.3");  // Channel 0 pan
    ASSERT_TRUE(result.success);

    result = cli.run_command("pd 0 2");     // Pulse duty cycle
    ASSERT_TRUE(result.success);

    result = cli.run_command("mc volume 0.9"); // Master volume
    ASSERT_TRUE(result.success);

    // Save preset with real-time settings
    result = cli.run_command("realtime-preset save cli_workflow_preset");
    ASSERT_TRUE(result.success);

    // Reset and reload
    result = cli.run_command("config-reset");
    ASSERT_TRUE(result.success);

    result = cli.run_command("realtime-preset load cli_workflow_preset");
    ASSERT_TRUE(result.success);

    // Verify settings are retrievable
    result = cli.run_command("realtime-get master_volume");
    ASSERT_TRUE(result.success);

    result = cli.run_command("realtime-stats");
    ASSERT_TRUE(result.success);
}

REGISTER_TEST(workflow_integration, configuration_change_during_playback) {
    // Test changing configuration parameters during active playback

    // Create engine with initial configuration
    nes_playback_engine::engine_config config;
    config.sample_rate = 22050;
    config.buffer_size = 512;
    config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;
    // Note: output filename is now handled by export_to_wav()

    auto engine = std::make_unique<nes_playback_engine>(config);
    ASSERT_TRUE(engine->initialize());

    // Create real-time controller
    auto realtime_controller = std::make_unique<nes_realtime::realtime_parameter_controller>();
    realtime_controller->set_playback_engine(engine.get());

    // Register test parameters
    nes_realtime::parameter_value volume_param(1.0f, 0.0f, 1.0f, "Master volume", "");
    ASSERT_TRUE(realtime_controller->register_parameter(
        nes_realtime::parameter_type::MASTER_VOLUME, 0, volume_param));

    nes_realtime::parameter_value ch_volume_param(1.0f, 0.0f, 1.0f, "Channel volume", "");
    ASSERT_TRUE(realtime_controller->register_parameter(
        nes_realtime::parameter_type::CHANNEL_VOLUME, 0, ch_volume_param));

    // Create and load test music (longer for this test)
    std::vector<uint8_t> midi_data;
    midi_data.insert(midi_data.end(), {'M', 'T', 'h', 'd'});
    midi_data.insert(midi_data.end(), {0x00, 0x00, 0x00, 0x06});
    midi_data.insert(midi_data.end(), {0x00, 0x00, 0x00, 0x01, 0x01, 0xE0});
    midi_data.insert(midi_data.end(), {'M', 'T', 'r', 'k'});

    std::vector<uint8_t> track_data = {
        0x00, 0x90, 0x3C, 0x64, // Note on C4
        0x40, 0x90, 0x40, 0x64, // Note on E4
        0x40, 0x90, 0x43, 0x64, // Note on G4
        0x60, 0x80, 0x3C, 0x40, // Note off C4
        0x60, 0x80, 0x40, 0x40, // Note off E4
        0x60, 0x80, 0x43, 0x40, // Note off G4
        0x00, 0xFF, 0x2F, 0x00  // End of track
    };

    uint32_t track_length = track_data.size();
    midi_data.push_back((track_length >> 24) & 0xFF);
    midi_data.push_back((track_length >> 16) & 0xFF);
    midi_data.push_back((track_length >> 8) & 0xFF);
    midi_data.push_back(track_length & 0xFF);
    midi_data.insert(midi_data.end(), track_data.begin(), track_data.end());

    std::ofstream midi_file("config_change_test.mid", std::ios::binary);
    midi_file.write(reinterpret_cast<const char*>(midi_data.data()), midi_data.size());
    midi_file.close();

    // Load and start playback
    midi_parser parser;
    music_data music;
    ASSERT_TRUE(parser.parse_file("config_change_test.mid", music));
    ASSERT_TRUE(engine->load_music_data(music));
    ASSERT_TRUE(engine->play());

    // Make parameter changes during playback
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Change master volume
    ASSERT_TRUE(realtime_controller->set_parameter(
        nes_realtime::parameter_type::MASTER_VOLUME, 0, 0.5f, "test"));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Change channel volume
    ASSERT_TRUE(realtime_controller->set_parameter(
        nes_realtime::parameter_type::CHANNEL_VOLUME, 0, 0.8f, "test"));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Change back to original values
    ASSERT_TRUE(realtime_controller->set_parameter(
        nes_realtime::parameter_type::MASTER_VOLUME, 0, 1.0f, "test"));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop playback
    engine->stop();
    ASSERT_FALSE(engine->is_playing());

    // Verify final parameter values
    auto final_volume = realtime_controller->get_parameter(
        nes_realtime::parameter_type::MASTER_VOLUME, 0);
    ASSERT_NEAR(1.0f, final_volume.value, 0.01f);

    engine->shutdown();
}

REGISTER_TEST(workflow_integration, multi_component_stress_test) {
    // Stress test involving multiple components working together

    // Set up CLI
    nes_cli::cli_config cli_config;
    cli_config.quiet = true;
    cli_config.audio_backend = "file";
    cli_config.output_file = "stress_test_output.wav";

    nes_cli cli(cli_config);

    // Configure via CLI (multiple operations)
    std::vector<std::string> commands = {
        "config-preset quality",
        "config-set audio.sample_rate 22050",
        "config-set audio.buffer_size 256",
        "cv 0 0.9",
        "cv 1 0.7",
        "cv 2 0.8",
        "cp 0 -0.5",
        "cp 1 0.5",
        "cp 2 0.0",
        "pd 0 1",
        "pd 1 2",
        "mc volume 0.8",
        "fc highpass 90",
        "fc lowpass 14000",
        "realtime-preset save stress_test_preset"
    };

    // Execute all commands
    for (const auto& command : commands) {
        auto result = cli.run_command(command);
        ASSERT_TRUE(result.success); // Failed command check
    }

    // Verify system is still responsive
    auto result = cli.run_command("realtime-stats");
    ASSERT_TRUE(result.success);

    result = cli.run_command("config-show");
    ASSERT_TRUE(result.success);

    // Load the preset we saved
    result = cli.run_command("realtime-preset load stress_test_preset");
    ASSERT_TRUE(result.success);

    // Reset and verify recovery
    result = cli.run_command("config-reset");
    ASSERT_TRUE(result.success);

    result = cli.run_command("help");
    ASSERT_TRUE(result.success);

    // Final validation
    result = cli.run_command("config-validate");
    ASSERT_TRUE(result.success);
}

REGISTER_TEST(workflow_integration, persistent_configuration_workflow) {
    // Test configuration persistence across different workflows

    const std::string config_file = "persistent_workflow_config.json";

    // Phase 1: Create and save configuration
    {
        nes_config::nes_configuration config; // Use default constructor
        config.audio.sample_rate = 48000;
        config.audio.enable_nonlinear_mixing = false;
        config.performance.enable_multithreading = true;
        config.performance.max_polyphony = 8;
        config.ui.verbose_output = true;

        ASSERT_TRUE(nes_config::nes_config_manager::save_configuration(config_file, config));
    }

    // Phase 2: Load via CLI and modify
    {
        nes_cli::cli_config cli_config;
        cli_config.quiet = true;
        nes_cli cli(cli_config);

        auto result = cli.run_command("config-load " + config_file);
        ASSERT_TRUE(result.success);

        // Verify loaded values via CLI queries
        result = cli.run_command("config-get audio.sample_rate");
        ASSERT_TRUE(result.success);

        // Modify via CLI
        result = cli.run_command("config-set audio.buffer_size 2048");
        ASSERT_TRUE(result.success);

        result = cli.run_command("config-set performance.max_polyphony 6");
        ASSERT_TRUE(result.success);

        // Save modified configuration
        result = cli.run_command("config-save " + config_file);
        ASSERT_TRUE(result.success);
    }

    // Phase 3: Verify modifications persisted
    {
        nes_config::nes_configuration loaded_config;
        ASSERT_TRUE(nes_config::nes_config_manager::load_configuration(config_file, loaded_config));

        // Verify original settings
        ASSERT_EQ(48000, loaded_config.audio.sample_rate);
        ASSERT_FALSE(loaded_config.audio.enable_nonlinear_mixing);
        ASSERT_TRUE(loaded_config.performance.enable_multithreading);
        ASSERT_TRUE(loaded_config.ui.verbose_output);

        // Verify CLI modifications
        ASSERT_EQ(2048, loaded_config.audio.buffer_size);
        ASSERT_EQ(6, loaded_config.performance.max_polyphony);
    }

    // Cleanup
    std::remove(config_file.c_str());
}