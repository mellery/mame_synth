#include "test_framework.h"
#include "music_parser.h"
#include "audio_device.h"
#include "register_mapping.h"
#include <vector>
#include <fstream>

REGISTER_TEST(integration, midi_to_register_pipeline) {
    // Create a simple MIDI file
    std::vector<uint8_t> midi_data;

    // MIDI header "MThd"
    midi_data.insert(midi_data.end(), {'M', 'T', 'h', 'd'});
    midi_data.insert(midi_data.end(), {0x00, 0x00, 0x00, 0x06});
    midi_data.insert(midi_data.end(), {0x00, 0x00, 0x00, 0x01, 0x01, 0xE0});

    // Track header "MTrk"
    midi_data.insert(midi_data.end(), {'M', 'T', 'r', 'k'});

    // Track data with multiple notes
    std::vector<uint8_t> track_data = {
        0x00,             // Delta time 0
        0x90, 0x3C, 0x64, // Note on C4, velocity 100
        0x00,             // Delta time 0
        0x91, 0x40, 0x50, // Note on E4 channel 1, velocity 80
        0x83, 0x60,       // Delta time 480
        0x80, 0x3C, 0x40, // Note off C4
        0x00,             // Delta time 0
        0x81, 0x40, 0x40, // Note off E4 channel 1
        0x00,             // Delta time 0
        0xFF, 0x2F, 0x00  // End of track
    };

    // Track length
    uint32_t track_length = track_data.size();
    midi_data.push_back((track_length >> 24) & 0xFF);
    midi_data.push_back((track_length >> 16) & 0xFF);
    midi_data.push_back((track_length >> 8) & 0xFF);
    midi_data.push_back(track_length & 0xFF);

    midi_data.insert(midi_data.end(), track_data.begin(), track_data.end());

    // Write MIDI file
    std::ofstream file("test_integration.mid", std::ios::binary);
    file.write(reinterpret_cast<const char*>(midi_data.data()), midi_data.size());
    file.close();

    // Parse MIDI file
    midi_parser parser;
    music_data music;
    ASSERT_TRUE(parser.parse_file("test_integration.mid", music));
    ASSERT_EQ(2, music.note_count());

    // Create NES APU register mapper
    auto mapper = register_mapping_factory::create_mapper(
        register_mapping_factory::NES_APU, 1789773);
    ASSERT_NE(nullptr, mapper.get());

    // Map notes to registers
    std::vector<uint8_t> registers;

    // Get first note and map it
    // Note: In real implementation, you'd iterate through the music data
    // For this test, we'll create equivalent notes
    music_note note1(0, 60, 100, 0, 480); // C4
    music_note note2(1, 64, 80, 0, 480);  // E4

    ASSERT_TRUE(mapper->map_note_on(note1, registers));
    ASSERT_TRUE(mapper->map_note_on(note2, registers));

    // Verify that both channels are enabled
    ASSERT_EQ(3, registers[0x15] & 0x03); // Channels 0 and 1 enabled
}

REGISTER_TEST(integration, audio_device_register_mapping_sync) {
    // Create audio device and register mapper for same device type
    nes_apu_device device("test", 1789773);
    auto mapper = register_mapping_factory::create_mapper(
        register_mapping_factory::NES_APU, 1789773);

    ASSERT_TRUE(device.initialize(44100));

    // Create test note
    music_note note(0, 60, 100, 0, 480);

    // Play note on audio device
    ASSERT_TRUE(device.play_note(note));
    ASSERT_TRUE(device.is_playing());

    // Map same note to registers
    std::vector<uint8_t> registers;
    ASSERT_TRUE(mapper->map_note_on(note, registers));

    // Both should agree that channel 0 is active
    // (This is a conceptual test - in real integration,
    // the audio device would use the register mapper)
    ASSERT_EQ(1, registers[0x15] & 0x01); // Channel 0 enabled in registers
}

REGISTER_TEST(integration, multi_device_audio_manager) {
    // Create audio device manager with multiple devices
    audio_device_manager manager;

    auto nes_device = std::make_unique<nes_apu_device>("nes", 1789773);
    auto snes_device = std::make_unique<snes_dsp_device>("snes", 24576000);

    manager.add_device(std::move(nes_device));
    manager.add_device(std::move(snes_device));
    ASSERT_TRUE(manager.initialize_all(44100));

    // Create register mappers for both devices
    auto nes_mapper = register_mapping_factory::create_mapper(
        register_mapping_factory::NES_APU, 1789773);
    auto snes_mapper = register_mapping_factory::create_mapper(
        register_mapping_factory::SNES_DSP, 24576000);

    // Play notes on both devices
    music_note nes_note(0, 60, 100, 0, 480);
    music_note snes_note(0, 67, 100, 0, 480);

    ASSERT_TRUE(manager.play_note_on_device("NES APU", nes_note));
    ASSERT_TRUE(manager.play_note_on_device("SNES S-DSP", snes_note));

    // Map notes to registers
    std::vector<uint8_t> nes_registers, snes_registers;
    ASSERT_TRUE(nes_mapper->map_note_on(nes_note, nes_registers));
    ASSERT_TRUE(snes_mapper->map_note_on(snes_note, snes_registers));

    // Verify register states
    ASSERT_EQ(1, nes_registers[0x15] & 0x01);   // NES channel 0 enabled
    ASSERT_EQ(1, snes_registers[0x4C] & 0x01);  // SNES voice 0 key on

    ASSERT_TRUE(manager.any_device_playing());
}

REGISTER_TEST(integration, program_and_control_changes) {
    // Test complete program and control change pipeline
    nes_apu_device device("test", 1789773);
    auto mapper = register_mapping_factory::create_mapper(
        register_mapping_factory::NES_APU, 1789773);

    ASSERT_TRUE(device.initialize(44100));

    // Activate channel first for program changes to take effect
    music_note note(0, 60, 100, 0, 480);
    ASSERT_TRUE(device.play_note(note));

    std::vector<uint8_t> registers;
    ASSERT_TRUE(mapper->map_note_on(note, registers));

    // Test program change
    music_program program(0, 100, 0); // Should map to duty cycle 3 (program >= 96)
    ASSERT_TRUE(device.set_program(program));
    ASSERT_TRUE(mapper->map_program_change(program, registers));

    // Both should agree on duty cycle setting
    // Extract duty cycle from register
    uint8_t duty = (registers[0x00] >> 6) & 3;
    ASSERT_EQ(3, duty);

    // Test volume control
    music_control volume(0, 7, 64);
    ASSERT_TRUE(device.set_control(volume));
    ASSERT_TRUE(mapper->map_control_change(volume, registers));

    // Volume should be mapped consistently
    uint8_t mapped_volume = registers[0x00] & 0x0F;
    uint8_t expected_volume = (64 * 15) / 127;
    ASSERT_EQ(expected_volume, mapped_volume);
}

REGISTER_TEST(integration, snes_stereo_processing) {
    // Test SNES stereo processing through both audio device and register mapping
    snes_dsp_device device("test", 24576000);
    auto mapper = register_mapping_factory::create_mapper(
        register_mapping_factory::SNES_DSP, 24576000);

    ASSERT_TRUE(device.initialize(44100));

    // Play a note
    music_note note(0, 67, 100, 0, 480);
    ASSERT_TRUE(device.play_note(note));

    std::vector<uint8_t> registers;
    ASSERT_TRUE(mapper->map_note_on(note, registers));

    // Initial volume should be equal for both channels
    ASSERT_EQ(registers[0x00], registers[0x01]); // Left == Right

    // Apply pan control (pan left)
    music_control pan_left(0, 10, 32);
    ASSERT_TRUE(device.set_control(pan_left));
    ASSERT_TRUE(mapper->map_control_change(pan_left, registers));

    // After panning left, right volume should be less than left
    ASSERT_GT(registers[0x00], registers[0x01]); // Left > Right
}

REGISTER_TEST(integration, midi_parsing_error_handling) {
    // Test error handling throughout the pipeline
    midi_parser parser;
    music_data data;

    // Test with non-existent file
    ASSERT_FALSE(parser.parse_file("nonexistent.mid", data));
    ASSERT_FALSE(parser.get_last_error().empty());

    // Test with invalid MIDI data
    std::vector<uint8_t> invalid_data = {'N', 'O', 'T', 'M', 'I', 'D', 'I'};
    ASSERT_FALSE(parser.parse_buffer(invalid_data, data));
    ASSERT_FALSE(parser.get_last_error().empty());
}

REGISTER_TEST(integration, register_mapping_edge_cases) {
    auto mapper = register_mapping_factory::create_mapper(
        register_mapping_factory::NES_APU, 1789773);

    std::vector<uint8_t> registers;

    // Test note off without note on
    ASSERT_FALSE(mapper->map_note_off(0, 60, registers));

    // Test note on extreme values
    music_note extreme_high(0, 127, 127, 0, 480);
    ASSERT_TRUE(mapper->map_note_on(extreme_high, registers));

    music_note extreme_low(0, 0, 1, 0, 480);
    ASSERT_TRUE(mapper->map_note_on(extreme_low, registers));

    // Test invalid channels
    music_note invalid_channel(255, 60, 100, 0, 480);
    ASSERT_TRUE(mapper->map_note_on(invalid_channel, registers)); // Should wrap channel
}

REGISTER_TEST(integration, performance_basic_timing) {
    // Basic performance test - not precise timing, just sanity check
    auto start = std::chrono::high_resolution_clock::now();

    // Perform a series of operations
    midi_parser parser;
    auto mapper = register_mapping_factory::create_mapper(
        register_mapping_factory::NES_APU, 1789773);
    audio_device_manager manager;

    auto device = std::make_unique<nes_apu_device>("perf_test", 1789773);
    manager.add_device(std::move(device));
    manager.initialize_all(44100);

    // Process multiple notes
    for (int i = 0; i < 100; ++i) {
        music_note note(i % 4, 60 + (i % 12), 100, 0, 480);
        std::vector<uint8_t> registers;

        mapper->map_note_on(note, registers);
        manager.play_note_on_device("NES APU", note);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (less than 100ms for 100 operations)
    ASSERT_LT(duration.count(), 100);
}

REGISTER_TEST(integration, memory_management) {
    // Test that memory management works correctly across components
    {
        audio_device_manager manager;

        // Add devices in inner scope
        {
            auto nes_device = std::make_unique<nes_apu_device>("mem_test", 1789773);
            auto snes_device = std::make_unique<snes_dsp_device>("mem_test2", 24576000);

            manager.add_device(std::move(nes_device));
            manager.add_device(std::move(snes_device));

            ASSERT_TRUE(manager.initialize_all(44100));
            ASSERT_EQ(2, manager.get_device_names().size());
        }

        // Devices should still be accessible
        ASSERT_NE(nullptr, manager.get_device("NES APU"));
        ASSERT_NE(nullptr, manager.get_device("SNES S-DSP"));

        // Clear devices explicitly
        manager.clear_devices();
        ASSERT_EQ(0, manager.get_device_names().size());
    }

    // Test register mapper memory management
    {
        std::vector<std::unique_ptr<register_mapper>> mappers;

        for (int i = 0; i < 10; ++i) {
            mappers.push_back(register_mapping_factory::create_mapper(
                register_mapping_factory::NES_APU, 1789773));
            mappers.push_back(register_mapping_factory::create_mapper(
                register_mapping_factory::SNES_DSP, 24576000));
        }

        ASSERT_EQ(20, mappers.size());

        // All mappers should be valid
        for (const auto& mapper : mappers) {
            ASSERT_NE(nullptr, mapper.get());
            ASSERT_FALSE(mapper->get_device_name().empty());
        }
    }
    // mappers vector destructor should clean up all mappers
}