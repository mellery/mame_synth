#include "test_framework.h"
#include "register_mapping.h"
#include "music_parser.h"
#include <cmath>

REGISTER_TEST(register_mapping, nes_apu_mapper_creation) {
    nes_apu_mapper mapper(1789773);

    ASSERT_EQ("NES APU", mapper.get_device_name());
    ASSERT_EQ(24, mapper.get_register_count());
    ASSERT_EQ(1789773, mapper.get_base_clock_rate());
}

REGISTER_TEST(register_mapping, nes_apu_mapper_note_mapping) {
    nes_apu_mapper mapper(1789773);

    music_note note(0, 60, 100, 0, 480); // C4 on pulse 1
    std::vector<uint8_t> registers;

    ASSERT_TRUE(mapper.map_note_on(note, registers));
    ASSERT_EQ(24, registers.size());

    // Check key registers
    ASSERT_NE(0, registers[0x00]); // $4000 - Control register should be set
    ASSERT_NE(0, registers[0x02]); // $4002 - Timer low should be set
    ASSERT_NE(0, registers[0x03]); // $4003 - Timer high should be set
    ASSERT_EQ(1, registers[0x15] & 0x01); // $4015 - Channel 0 should be enabled
}

REGISTER_TEST(register_mapping, nes_apu_mapper_note_off) {
    nes_apu_mapper mapper(1789773);

    music_note note(0, 60, 100, 0, 480);
    std::vector<uint8_t> registers;

    // Map note on first
    mapper.map_note_on(note, registers);
    ASSERT_EQ(1, registers[0x15] & 0x01); // Channel should be enabled

    // Map note off
    ASSERT_TRUE(mapper.map_note_off(0, 60, registers));
    ASSERT_EQ(0, registers[0x15] & 0x01); // Channel should be disabled
}

REGISTER_TEST(register_mapping, nes_apu_mapper_program_change) {
    nes_apu_mapper mapper(1789773);

    // Activate channel 0 first so program changes take effect
    music_note note(0, 60, 100, 0, 480);
    std::vector<uint8_t> registers;
    mapper.map_note_on(note, registers);

    // Test different duty cycles
    music_program program1(0, 10, 0);  // Should map to duty 0 (12.5%)
    music_program program2(0, 40, 0);  // Should map to duty 1 (25%)
    music_program program3(0, 70, 0);  // Should map to duty 2 (50%)
    music_program program4(0, 100, 0); // Should map to duty 3 (25% negated)

    mapper.map_program_change(program1, registers);
    uint8_t duty1 = (registers[0x00] >> 6) & 3;
    ASSERT_EQ(0, duty1);

    mapper.map_program_change(program2, registers);
    uint8_t duty2 = (registers[0x00] >> 6) & 3;
    ASSERT_EQ(1, duty2);

    mapper.map_program_change(program3, registers);
    uint8_t duty3 = (registers[0x00] >> 6) & 3;
    ASSERT_EQ(2, duty3);

    mapper.map_program_change(program4, registers);
    uint8_t duty4 = (registers[0x00] >> 6) & 3;
    ASSERT_EQ(3, duty4);
}

REGISTER_TEST(register_mapping, nes_apu_mapper_control_change) {
    nes_apu_mapper mapper(1789773);

    // Test volume control
    music_control volume_control(0, 7, 100);
    std::vector<uint8_t> registers;

    mapper.map_control_change(volume_control, registers);

    // Volume should be in lower 4 bits of control register
    uint8_t volume = registers[0x00] & 0x0F;
    ASSERT_EQ((100 * 15) / 127, volume);
}

REGISTER_TEST(register_mapping, nes_apu_mapper_frequency_calculation) {
    nes_apu_mapper mapper(1789773);

    music_note c4(0, 60, 100, 0, 480); // C4 = 261.626 Hz
    std::vector<uint8_t> registers;

    mapper.map_note_on(c4, registers);

    // Calculate expected timer value
    double frequency = 440.0 * std::pow(2.0, (60 - 69) / 12.0); // C4 frequency
    uint32_t expected_timer = static_cast<uint32_t>((1789773 / (16.0 * frequency)) - 1);

    // Extract timer value from registers
    uint16_t actual_timer = registers[0x02] | ((registers[0x03] & 0x07) << 8);

    ASSERT_EQ(expected_timer & 0x7FF, actual_timer); // 11-bit timer
}

REGISTER_TEST(register_mapping, nes_apu_mapper_triangle_channel) {
    nes_apu_mapper mapper(1789773);

    music_note note(2, 67, 100, 0, 480); // G4 on triangle channel
    std::vector<uint8_t> registers;

    ASSERT_TRUE(mapper.map_note_on(note, registers));

    // Triangle channel registers ($4008-$400B)
    ASSERT_NE(0, registers[0x08]); // Linear counter control
    ASSERT_NE(0, registers[0x0A]); // Timer low
    ASSERT_NE(0, registers[0x0B]); // Timer high
    ASSERT_EQ(1, (registers[0x15] >> 2) & 1); // Triangle channel enabled
}

REGISTER_TEST(register_mapping, nes_apu_mapper_noise_channel) {
    nes_apu_mapper mapper(1789773);

    music_note note(3, 80, 100, 0, 480); // High note on noise channel
    std::vector<uint8_t> registers;

    ASSERT_TRUE(mapper.map_note_on(note, registers));

    // Noise channel registers ($400C-$400F)
    ASSERT_NE(0, registers[0x0C]); // Envelope control
    ASSERT_NE(0, registers[0x0E]); // Period and waveform
    ASSERT_NE(0, registers[0x0F]); // Length counter
    ASSERT_EQ(1, (registers[0x15] >> 3) & 1); // Noise channel enabled
}

REGISTER_TEST(register_mapping, snes_dsp_mapper_creation) {
    snes_dsp_mapper mapper(24576000);

    ASSERT_EQ("SNES S-DSP", mapper.get_device_name());
    ASSERT_EQ(128, mapper.get_register_count());
    ASSERT_EQ(24576000, mapper.get_base_clock_rate());
}

REGISTER_TEST(register_mapping, snes_dsp_mapper_note_mapping) {
    snes_dsp_mapper mapper(24576000);

    music_note note(2, 67, 100, 0, 480); // G4 on voice 2
    std::vector<uint8_t> registers;

    ASSERT_TRUE(mapper.map_note_on(note, registers));
    ASSERT_EQ(128, registers.size());

    // Check voice 2 registers ($20-$29)
    ASSERT_NE(0, registers[0x20]); // Left volume
    ASSERT_NE(0, registers[0x21]); // Right volume
    ASSERT_NE(0, registers[0x22]); // Pitch low
    ASSERT_NE(0, registers[0x23]); // Pitch high
    ASSERT_EQ(4, registers[0x4C]);  // Key on for voice 2 (bit 2)
}

REGISTER_TEST(register_mapping, snes_dsp_mapper_note_off) {
    snes_dsp_mapper mapper(24576000);

    music_note note(3, 72, 100, 0, 480); // C5 on voice 3
    std::vector<uint8_t> registers;

    // Map note on first
    mapper.map_note_on(note, registers);
    ASSERT_EQ(8, registers[0x4C]); // Voice 3 key on (bit 3)

    // Map note off
    ASSERT_TRUE(mapper.map_note_off(3, 72, registers));
    ASSERT_EQ(8, registers[0x5C]); // Voice 3 key off (bit 3)
}

REGISTER_TEST(register_mapping, snes_dsp_mapper_program_change) {
    snes_dsp_mapper mapper(24576000);

    music_program program(5, 42, 0); // Voice 5, sample 42
    std::vector<uint8_t> registers;

    ASSERT_TRUE(mapper.map_program_change(program, registers));

    // Voice 5 source register ($54)
    ASSERT_EQ(42, registers[0x54]);
}

REGISTER_TEST(register_mapping, snes_dsp_mapper_volume_control) {
    snes_dsp_mapper mapper(24576000);

    music_control volume_control(1, 7, 100); // Voice 1, volume 100
    std::vector<uint8_t> registers;

    ASSERT_TRUE(mapper.map_control_change(volume_control, registers));

    // Voice 1 volume registers ($10, $11)
    uint8_t expected_volume = (100 * 0x7F) / 127;
    ASSERT_EQ(expected_volume, registers[0x10]); // Left volume
    ASSERT_EQ(expected_volume, registers[0x11]); // Right volume
}

REGISTER_TEST(register_mapping, snes_dsp_mapper_pan_control) {
    snes_dsp_mapper mapper(24576000);

    // Set initial volume
    music_control volume_control(0, 7, 100);
    std::vector<uint8_t> registers;
    mapper.map_control_change(volume_control, registers);

    uint8_t base_volume = registers[0x00];

    // Pan left
    music_control pan_left(0, 10, 32);
    mapper.map_control_change(pan_left, registers);

    ASSERT_EQ(base_volume, registers[0x00]); // Left should be full
    ASSERT_LT(registers[0x01], base_volume); // Right should be reduced

    // Pan right
    music_control pan_right(0, 10, 96);
    mapper.map_control_change(pan_right, registers);

    ASSERT_LT(registers[0x00], base_volume); // Left should be reduced
    ASSERT_EQ(base_volume, registers[0x01]); // Right should be full
}

REGISTER_TEST(register_mapping, snes_dsp_mapper_echo_control) {
    snes_dsp_mapper mapper(24576000);

    // Enable echo for voice 4
    music_control echo_on(4, 91, 80);
    std::vector<uint8_t> registers;

    mapper.map_control_change(echo_on, registers);
    ASSERT_EQ(16, registers[0x4D] & 16); // Echo flag for voice 4 (bit 4)

    // Disable echo for voice 4
    music_control echo_off(4, 91, 40);
    mapper.map_control_change(echo_off, registers);
    ASSERT_EQ(0, registers[0x4D] & 16); // Echo flag should be cleared
}

REGISTER_TEST(register_mapping, snes_dsp_mapper_pitch_calculation) {
    snes_dsp_mapper mapper(24576000);

    music_note c4(0, 60, 100, 0, 480); // C4 = 261.626 Hz
    std::vector<uint8_t> registers;

    mapper.map_note_on(c4, registers);

    // Calculate expected pitch value
    double frequency = 440.0 * std::pow(2.0, (60 - 69) / 12.0); // C4 frequency
    double pitch_multiplier = frequency / 261.626;
    uint16_t expected_pitch = static_cast<uint16_t>(0x1000 * pitch_multiplier);

    // Extract pitch value from registers
    uint16_t actual_pitch = registers[0x02] | ((registers[0x03] & 0x3F) << 8);

    ASSERT_EQ(expected_pitch & 0x3FFF, actual_pitch); // 14-bit pitch
}

REGISTER_TEST(register_mapping, register_mapping_factory) {
    // Test NES APU creation
    auto nes_mapper = register_mapping_factory::create_mapper(
        register_mapping_factory::NES_APU, 1789773);
    ASSERT_NE(nullptr, nes_mapper.get());
    ASSERT_EQ("NES APU", nes_mapper->get_device_name());

    // Test SNES S-DSP creation
    auto snes_mapper = register_mapping_factory::create_mapper(
        register_mapping_factory::SNES_DSP, 24576000);
    ASSERT_NE(nullptr, snes_mapper.get());
    ASSERT_EQ("SNES S-DSP", snes_mapper->get_device_name());

    // Test default clock rates
    auto nes_default = register_mapping_factory::create_mapper(
        register_mapping_factory::NES_APU);
    ASSERT_EQ(nes_apu_mapper::BASE_CLOCK_NTSC, nes_default->get_base_clock_rate());

    auto snes_default = register_mapping_factory::create_mapper(
        register_mapping_factory::SNES_DSP);
    ASSERT_EQ(snes_dsp_mapper::BASE_CLOCK, snes_default->get_base_clock_rate());
}

REGISTER_TEST(register_mapping, factory_supported_devices) {
    auto devices = register_mapping_factory::supported_devices();

    ASSERT_EQ(2, devices.size());

    bool found_nes = false, found_snes = false;
    for (auto device : devices) {
        if (device == register_mapping_factory::NES_APU) found_nes = true;
        if (device == register_mapping_factory::SNES_DSP) found_snes = true;
    }

    ASSERT_TRUE(found_nes);
    ASSERT_TRUE(found_snes);
}

REGISTER_TEST(register_mapping, factory_device_names) {
    ASSERT_EQ("NES APU",
              register_mapping_factory::device_type_name(register_mapping_factory::NES_APU));
    ASSERT_EQ("SNES S-DSP",
              register_mapping_factory::device_type_name(register_mapping_factory::SNES_DSP));
}

REGISTER_TEST(register_mapping, register_snapshot_creation) {
    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
    register_snapshot snapshot("Test Device", 1000, test_data, "Test snapshot");

    ASSERT_EQ("Test Device", snapshot.device_name);
    ASSERT_EQ(1000, snapshot.timestamp_ms);
    ASSERT_EQ(4, snapshot.register_data.size());
    ASSERT_EQ("Test snapshot", snapshot.description);
    ASSERT_EQ(0x01, snapshot.register_data[0]);
    ASSERT_EQ(0x04, snapshot.register_data[3]);
}

REGISTER_TEST(register_mapping, register_diff_compare) {
    std::vector<uint8_t> before_data = {0x00, 0x01, 0x02, 0x03};
    std::vector<uint8_t> after_data =  {0x00, 0x05, 0x02, 0x07};

    register_snapshot before("Test", 0, before_data);
    register_snapshot after("Test", 100, after_data);

    auto changes = register_diff::compare(before, after);

    ASSERT_EQ(2, changes.size());

    // Check first change (address 1: 0x01 -> 0x05)
    ASSERT_EQ(1, changes[0].address);
    ASSERT_EQ(0x01, changes[0].old_value);
    ASSERT_EQ(0x05, changes[0].new_value);

    // Check second change (address 3: 0x03 -> 0x07)
    ASSERT_EQ(3, changes[1].address);
    ASSERT_EQ(0x03, changes[1].old_value);
    ASSERT_EQ(0x07, changes[1].new_value);
}

REGISTER_TEST(register_mapping, register_diff_no_changes) {
    std::vector<uint8_t> data = {0x10, 0x20, 0x30, 0x40};

    register_snapshot before("Test", 0, data);
    register_snapshot after("Test", 100, data);

    auto changes = register_diff::compare(before, after);

    ASSERT_EQ(0, changes.size());
}

REGISTER_TEST(register_mapping, register_diff_different_devices) {
    std::vector<uint8_t> data = {0x01, 0x02};

    register_snapshot device1("Device1", 0, data);
    register_snapshot device2("Device2", 100, data);

    auto changes = register_diff::compare(device1, device2);

    ASSERT_EQ(0, changes.size()); // Should return empty for different devices
}