#include "test_framework.h"
#include "audio_device.h"
#include "music_parser.h"

REGISTER_TEST(audio_device, nes_apu_device_creation) {
    nes_apu_device device("test_nes", 1789773);

    ASSERT_EQ("NES APU", device.get_name());
    ASSERT_EQ("Nintendo Entertainment System Audio Processing Unit", device.get_description());
    ASSERT_EQ(5, device.get_channel_count());
}

REGISTER_TEST(audio_device, nes_apu_device_initialization) {
    nes_apu_device device("test_nes", 1789773);

    ASSERT_TRUE(device.initialize(44100));
    ASSERT_EQ(44100, device.get_sample_rate());

    // Test double initialization (should keep original rate)
    ASSERT_TRUE(device.initialize(48000));
    ASSERT_EQ(44100, device.get_sample_rate()); // Should still be 44100
}

REGISTER_TEST(audio_device, nes_apu_device_note_playback) {
    nes_apu_device device("test_nes", 1789773);
    ASSERT_TRUE(device.initialize(44100));

    music_note note(0, 60, 100, 0, 480); // C4 on channel 0

    ASSERT_TRUE(device.play_note(note));
    ASSERT_TRUE(device.is_playing());

    ASSERT_TRUE(device.stop_note(0, 60));
    ASSERT_FALSE(device.is_playing());
}

REGISTER_TEST(audio_device, nes_apu_device_program_change) {
    nes_apu_device device("test_nes", 1789773);
    ASSERT_TRUE(device.initialize(44100));

    music_program program(0, 75, 0); // Should map to duty cycle 3
    ASSERT_TRUE(device.set_program(program));

    music_program program2(1, 25, 0); // Should map to duty cycle 1
    ASSERT_TRUE(device.set_program(program2));

    // Triangle channel doesn't support program changes (duty cycles)
    music_program program3(2, 50, 0);
    ASSERT_TRUE(device.set_program(program3)); // Should return true but do nothing
}

REGISTER_TEST(audio_device, nes_apu_device_volume_control) {
    nes_apu_device device("test_nes", 1789773);
    ASSERT_TRUE(device.initialize(44100));

    device.set_channel_volume(0, 100);
    device.set_channel_volume(1, 64);
    device.set_master_volume(200);

    // Test muting
    device.mute_channel(0, true);
    device.mute_channel(0, false);
}

REGISTER_TEST(audio_device, nes_apu_device_specific_features) {
    nes_apu_device device("test_nes", 1789773);
    ASSERT_TRUE(device.initialize(44100));

    // Test pulse duty cycle
    device.set_pulse_duty_cycle(0, 0); // 12.5%
    device.set_pulse_duty_cycle(0, 1); // 25%
    device.set_pulse_duty_cycle(0, 2); // 50%
    device.set_pulse_duty_cycle(0, 3); // 25% negated

    // Test triangle linear counter
    device.set_triangle_linear_counter(0x40);

    // Test noise mode
    device.set_noise_mode(true);  // Short mode
    device.set_noise_mode(false); // Long mode
}

REGISTER_TEST(audio_device, snes_dsp_device_creation) {
    snes_dsp_device device("test_snes", 24576000);

    ASSERT_EQ("SNES S-DSP", device.get_name());
    ASSERT_EQ("Super Nintendo S-DSP Sound Processor", device.get_description());
    ASSERT_EQ(8, device.get_channel_count());
}

REGISTER_TEST(audio_device, snes_dsp_device_initialization) {
    snes_dsp_device device("test_snes", 24576000);

    ASSERT_TRUE(device.initialize(44100));
    ASSERT_EQ(44100, device.get_sample_rate());
}

REGISTER_TEST(audio_device, snes_dsp_device_note_playback) {
    snes_dsp_device device("test_snes", 24576000);
    ASSERT_TRUE(device.initialize(44100));

    music_note note(2, 67, 100, 0, 480); // G4 on voice 2

    ASSERT_TRUE(device.play_note(note));
    ASSERT_TRUE(device.is_playing());

    ASSERT_TRUE(device.stop_note(2, 67));
    ASSERT_FALSE(device.is_playing());
}

REGISTER_TEST(audio_device, snes_dsp_device_program_change) {
    snes_dsp_device device("test_snes", 24576000);
    ASSERT_TRUE(device.initialize(44100));

    music_program program(0, 42, 0); // Should map to sample 42
    ASSERT_TRUE(device.set_program(program));

    music_program program2(7, 100, 0); // Voice 7, sample 100
    ASSERT_TRUE(device.set_program(program2));
}

REGISTER_TEST(audio_device, snes_dsp_device_control_change) {
    snes_dsp_device device("test_snes", 24576000);
    ASSERT_TRUE(device.initialize(44100));

    // Test volume control
    music_control volume(0, 7, 100);
    ASSERT_TRUE(device.set_control(volume));

    // Test pan control
    music_control pan_left(1, 10, 32);  // Pan left
    ASSERT_TRUE(device.set_control(pan_left));

    music_control pan_right(1, 10, 96); // Pan right
    ASSERT_TRUE(device.set_control(pan_right));

    // Test reverb control
    music_control reverb(2, 91, 80);
    ASSERT_TRUE(device.set_control(reverb));
}

REGISTER_TEST(audio_device, snes_dsp_device_specific_features) {
    snes_dsp_device device("test_snes", 24576000);
    ASSERT_TRUE(device.initialize(44100));

    // Test echo
    device.set_echo_enable(0, true);
    device.set_echo_enable(1, false);

    // Test pitch modulation
    device.set_pitch_modulation(2, true);
    device.set_pitch_modulation(3, false);

    // Test noise
    device.set_noise_enable(4, true);
    device.set_noise_enable(5, false);
}

REGISTER_TEST(audio_device, audio_device_manager_creation) {
    audio_device_manager manager;

    ASSERT_EQ(0, manager.get_device_names().size());
    ASSERT_FALSE(manager.any_device_playing());
}

REGISTER_TEST(audio_device, audio_device_manager_add_devices) {
    audio_device_manager manager;

    auto nes_device = std::make_unique<nes_apu_device>("nes", 1789773);
    auto snes_device = std::make_unique<snes_dsp_device>("snes", 24576000);

    ASSERT_TRUE(manager.add_device(std::move(nes_device)));
    ASSERT_TRUE(manager.add_device(std::move(snes_device)));

    auto device_names = manager.get_device_names();
    ASSERT_EQ(2, device_names.size());

    // Check device retrieval
    ASSERT_NE(nullptr, manager.get_device("NES APU"));
    ASSERT_NE(nullptr, manager.get_device("SNES S-DSP"));
    ASSERT_EQ(nullptr, manager.get_device("NonExistent"));
}

REGISTER_TEST(audio_device, audio_device_manager_initialization) {
    audio_device_manager manager;

    auto nes_device = std::make_unique<nes_apu_device>("nes", 1789773);
    auto snes_device = std::make_unique<snes_dsp_device>("snes", 24576000);

    manager.add_device(std::move(nes_device));
    manager.add_device(std::move(snes_device));

    ASSERT_TRUE(manager.initialize_all(44100));
}

REGISTER_TEST(audio_device, audio_device_manager_note_routing) {
    audio_device_manager manager;

    auto nes_device = std::make_unique<nes_apu_device>("nes", 1789773);
    auto snes_device = std::make_unique<snes_dsp_device>("snes", 24576000);

    manager.add_device(std::move(nes_device));
    manager.add_device(std::move(snes_device));
    manager.initialize_all(44100);

    // Test note routing
    music_note nes_note(0, 60, 100, 0, 480);
    ASSERT_TRUE(manager.play_note_on_device("NES APU", nes_note));

    music_note snes_note(2, 67, 100, 0, 480);
    ASSERT_TRUE(manager.play_note_on_device("SNES S-DSP", snes_note));

    ASSERT_TRUE(manager.any_device_playing());

    // Test stopping notes
    ASSERT_TRUE(manager.stop_note_on_device("NES APU", 0, 60));
    ASSERT_TRUE(manager.stop_note_on_device("SNES S-DSP", 2, 67));

    ASSERT_FALSE(manager.any_device_playing());
}

REGISTER_TEST(audio_device, audio_device_manager_program_routing) {
    audio_device_manager manager;

    auto nes_device = std::make_unique<nes_apu_device>("nes", 1789773);
    manager.add_device(std::move(nes_device));
    manager.initialize_all(44100);

    music_program program(0, 75, 0);
    ASSERT_TRUE(manager.set_program_on_device("NES APU", program));

    // Test non-existent device
    ASSERT_FALSE(manager.set_program_on_device("NonExistent", program));
}

REGISTER_TEST(audio_device, audio_device_manager_control_routing) {
    audio_device_manager manager;

    auto snes_device = std::make_unique<snes_dsp_device>("snes", 24576000);
    manager.add_device(std::move(snes_device));
    manager.initialize_all(44100);

    music_control control(0, 7, 100);
    ASSERT_TRUE(manager.set_control_on_device("SNES S-DSP", control));

    // Test non-existent device
    ASSERT_FALSE(manager.set_control_on_device("NonExistent", control));
}

REGISTER_TEST(audio_device, audio_device_manager_mixed_output) {
    audio_device_manager manager;

    auto nes_device = std::make_unique<nes_apu_device>("nes", 1789773);
    auto snes_device = std::make_unique<snes_dsp_device>("snes", 24576000);

    manager.add_device(std::move(nes_device));
    manager.add_device(std::move(snes_device));
    manager.initialize_all(44100);

    // Test mixed audio generation
    std::vector<int16_t> buffer(1024);
    manager.generate_mixed_samples(buffer.data(), buffer.size());

    // Buffer should be silent (all zeros) since no actual audio generation is implemented
    bool all_zeros = true;
    for (auto sample : buffer) {
        if (sample != 0) {
            all_zeros = false;
            break;
        }
    }
    ASSERT_TRUE(all_zeros);
}

REGISTER_TEST(audio_device, audio_device_manager_global_volume) {
    audio_device_manager manager;

    ASSERT_EQ(0xFF, manager.get_global_volume());

    manager.set_global_volume(128);
    ASSERT_EQ(128, manager.get_global_volume());

    manager.set_global_volume(0);
    ASSERT_EQ(0, manager.get_global_volume());
}