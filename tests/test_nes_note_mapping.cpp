#include "test_framework.h"
#include "../src/nes_note_mapping.h"
#include <cmath>
#include <string>

REGISTER_TEST(nes_note_mapping, basic_note_mapping) {
    nes_note_mapping::nes_note_mapper mapper(nes_note_mapping::nes_note_mapper::region_t::NTSC);

    // Test middle C (MIDI note 60)
    uint8_t middle_c = 60;
    uint16_t pulse_timer = mapper.note_to_timer(middle_c, nes_note_mapping::nes_note_mapper::channel_type_t::PULSE);
    uint16_t triangle_timer = mapper.note_to_timer(middle_c, nes_note_mapping::nes_note_mapper::channel_type_t::TRIANGLE);

    // Triangle timer should be approximately half the pulse timer (due to divider difference: 32 vs 16)
    ASSERT_LT(abs(static_cast<int>(triangle_timer * 2) - static_cast<int>(pulse_timer)), 3);

    // Test A4 (MIDI note 69 = 440 Hz)
    uint8_t a4 = 69;
    uint16_t a4_timer = mapper.note_to_timer(a4, nes_note_mapping::nes_note_mapper::channel_type_t::PULSE);
    double a4_freq = mapper.timer_to_frequency(a4_timer, nes_note_mapping::nes_note_mapper::channel_type_t::PULSE);

    // Frequency should be close to 440 Hz (within 2Hz tolerance due to timer quantization)
    ASSERT_NEAR(a4_freq, 440.0, 2.0);

    // Test note names
    ASSERT_EQ(std::string("C4"), std::string(mapper.note_to_name(middle_c)));
    ASSERT_EQ(std::string("A4"), std::string(mapper.note_to_name(a4)));
}

REGISTER_TEST(nes_note_mapping, playable_ranges) {
    nes_note_mapping::nes_note_mapper mapper(nes_note_mapping::nes_note_mapper::region_t::NTSC);

    // Test optimal ranges
    auto pulse_range = mapper.get_optimal_range(nes_note_mapping::nes_note_mapper::channel_type_t::PULSE);
    auto triangle_range = mapper.get_optimal_range(nes_note_mapping::nes_note_mapper::channel_type_t::TRIANGLE);
    auto noise_range = mapper.get_optimal_range(nes_note_mapping::nes_note_mapper::channel_type_t::NOISE);

    // Ranges should be reasonable
    ASSERT_GT(pulse_range.max_note, pulse_range.min_note);
    ASSERT_GT(triangle_range.max_note, triangle_range.min_note);
    ASSERT_GT(noise_range.max_note, noise_range.min_note);

    // Test playability
    ASSERT_TRUE(mapper.is_note_playable(60, nes_note_mapping::nes_note_mapper::channel_type_t::PULSE)); // Middle C should be playable
    ASSERT_TRUE(mapper.is_note_playable(69, nes_note_mapping::nes_note_mapper::channel_type_t::PULSE)); // A4 should be playable
    ASSERT_FALSE(mapper.is_note_playable(0, nes_note_mapping::nes_note_mapper::channel_type_t::PULSE));  // Very low note should not be playable
    ASSERT_FALSE(mapper.is_note_playable(127, nes_note_mapping::nes_note_mapper::channel_type_t::PULSE)); // Very high note should not be playable
}

REGISTER_TEST(nes_note_mapping, noise_mapping) {
    nes_note_mapping::nes_note_mapper mapper(nes_note_mapping::nes_note_mapper::region_t::NTSC);

    // Test noise period mapping
    uint8_t low_note = 36;   // C2
    uint8_t mid_note = 60;   // C4
    uint8_t high_note = 84;  // C6

    uint8_t low_period = mapper.note_to_noise_period(low_note);
    uint8_t mid_period = mapper.note_to_noise_period(mid_note);
    uint8_t high_period = mapper.note_to_noise_period(high_note);

    // Higher notes should map to higher period indices (shorter periods)
    ASSERT_GT(high_period, mid_period);
    ASSERT_GT(mid_period, low_period);

    // Periods should be in valid range (0-15)
    ASSERT_LT(low_period, 16);
    ASSERT_LT(mid_period, 16);
    ASSERT_LT(high_period, 16);
}

REGISTER_TEST(nes_note_mapping, pitch_bend) {
    nes_note_mapping::nes_note_mapper mapper(nes_note_mapping::nes_note_mapper::region_t::NTSC);

    uint8_t base_note = 60; // Middle C
    uint16_t base_timer = mapper.note_to_timer(base_note, nes_note_mapping::nes_note_mapper::channel_type_t::PULSE);

    // Test pitch bend up (positive cents)
    uint16_t bend_up_timer = mapper.note_to_timer_with_bend(base_note,
        nes_note_mapping::nes_note_mapper::channel_type_t::PULSE, 100); // 100 cents = 1 semitone up

    // Test pitch bend down (negative cents)
    uint16_t bend_down_timer = mapper.note_to_timer_with_bend(base_note,
        nes_note_mapping::nes_note_mapper::channel_type_t::PULSE, -100); // 100 cents = 1 semitone down

    // Bending up should decrease timer value (higher frequency)
    // Bending down should increase timer value (lower frequency)
    ASSERT_LT(bend_up_timer, base_timer);
    ASSERT_GT(bend_down_timer, base_timer);
}

REGISTER_TEST(nes_note_mapping, region_differences) {
    nes_note_mapping::nes_note_mapper ntsc_mapper(nes_note_mapping::nes_note_mapper::region_t::NTSC);
    nes_note_mapping::nes_note_mapper pal_mapper(nes_note_mapping::nes_note_mapper::region_t::PAL);

    uint8_t test_note = 69; // A4
    uint16_t ntsc_timer = ntsc_mapper.note_to_timer(test_note, nes_note_mapping::nes_note_mapper::channel_type_t::PULSE);
    uint16_t pal_timer = pal_mapper.note_to_timer(test_note, nes_note_mapping::nes_note_mapper::channel_type_t::PULSE);

    // PAL and NTSC should produce different timer values due to different clock rates
    ASSERT_NE(ntsc_timer, pal_timer);

    // PAL has a slower clock, so should produce smaller timer values for the same frequency
    ASSERT_LT(pal_timer, ntsc_timer);
}

REGISTER_TEST(nes_note_mapping, lookup_tables) {
    using namespace nes_note_mapping::note_tables;

    // Test that lookup tables are properly initialized
    ASSERT_EQ(128, MIDI_FREQUENCIES.size());
    ASSERT_EQ(128, NTSC_PULSE_TIMERS.size());
    ASSERT_EQ(128, NTSC_TRIANGLE_TIMERS.size());
    ASSERT_EQ(128, NOTE_NAMES.size());

    // Test A4 frequency (MIDI note 69 = 440 Hz)
    double a4_freq = MIDI_FREQUENCIES[69];
    ASSERT_NEAR(a4_freq, 440.0, 0.001);

    // Test note names
    ASSERT_EQ(std::string("C4"), std::string(NOTE_NAMES[60]));  // Middle C
    ASSERT_EQ(std::string("A4"), std::string(NOTE_NAMES[69]));  // A above middle C

    // Triangle timers should be approximately half pulse timers (32 divider vs 16 divider)
    for (int i = 24; i < 96; ++i) { // Test reasonable range
        uint16_t pulse_timer = NTSC_PULSE_TIMERS[i];
        uint16_t triangle_timer = NTSC_TRIANGLE_TIMERS[i];

        if (pulse_timer > 0 && triangle_timer > 0) {
            double ratio = static_cast<double>(triangle_timer) / pulse_timer;
            // Should be around 0.5, but clamping can cause deviations at extreme frequencies
            ASSERT_GT(ratio, 0.2);
            ASSERT_LT(ratio, 1.0);
        }
    }
}