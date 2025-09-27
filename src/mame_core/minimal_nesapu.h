#pragma once

#include "mame_minimal.h"

/**
 * Minimal NES APU Device Implementation
 *
 * This provides a simplified implementation of the MAME NES APU device
 * that maintains compatibility with the MAME nesapu_device interface
 * while working with our minimal MAME core.
 *
 * Based on MAME's nesapu_device but with minimal dependencies.
 */

// NES APU register definitions
enum nesapu_register {
    NES_APU_PULSE1_0     = 0x00,  // $4000: Pulse 1 duty/envelope/volume
    NES_APU_PULSE1_1     = 0x01,  // $4001: Pulse 1 sweep
    NES_APU_PULSE1_2     = 0x02,  // $4002: Pulse 1 timer low
    NES_APU_PULSE1_3     = 0x03,  // $4003: Pulse 1 length/timer high

    NES_APU_PULSE2_0     = 0x04,  // $4004: Pulse 2 duty/envelope/volume
    NES_APU_PULSE2_1     = 0x05,  // $4005: Pulse 2 sweep
    NES_APU_PULSE2_2     = 0x06,  // $4006: Pulse 2 timer low
    NES_APU_PULSE2_3     = 0x07,  // $4007: Pulse 2 length/timer high

    NES_APU_TRIANGLE_0   = 0x08,  // $4008: Triangle linear counter
    NES_APU_TRIANGLE_2   = 0x0A,  // $400A: Triangle timer low
    NES_APU_TRIANGLE_3   = 0x0B,  // $400B: Triangle length/timer high

    NES_APU_NOISE_0      = 0x0C,  // $400C: Noise envelope/volume
    NES_APU_NOISE_2      = 0x0E,  // $400E: Noise period
    NES_APU_NOISE_3      = 0x0F,  // $400F: Noise length

    NES_APU_STATUS       = 0x15,  // $4015: Channel enable/status
    NES_APU_FRAME        = 0x17   // $4017: Frame counter
};

// Forward declaration for compatibility with existing integration
class nesapu_device;
class device_t;
class device_sound_interface;

/**
 * Minimal NES APU device implementation
 * Compatible with MAME nesapu_device interface but using minimal core
 */
class minimal_nesapu_device : public minimal_device_t, public minimal_device_sound_interface {
public:
    minimal_nesapu_device(minimal_machine_config* config, const char* tag, u32 clock);
    virtual ~minimal_nesapu_device();

    // Device lifecycle (override minimal_device_t)
    void device_start() override;
    void device_reset() override;
    void device_stop() override;

    // Register interface (compatible with MAME nesapu_device)
    void write(offs_t offset, u8 data);
    u8 status_r();

    // Sound interface (override minimal_device_sound_interface)
    void sound_stream_update(s16* buffer, size_t samples) override;
    u32 sample_rate() const override { return m_sample_rate; }

    // IRQ callback support (compatible with MAME)
    auto irq() { return m_irq_handler.bind(); }
    auto mem_read() { return m_mem_read_cb.bind(); }

    // Clock management
    void device_clock_changed();

    // Get MAME-compatible device pointer for integration layer
    device_t* get_device_t() { return reinterpret_cast<device_t*>(this); }
    device_sound_interface* get_sound_interface() {
        return reinterpret_cast<device_sound_interface*>(this);
    }

    // Compatibility wrapper - allows casting to nesapu_device*
    nesapu_device* as_nesapu_device() {
        return reinterpret_cast<nesapu_device*>(this);
    }

private:
    // Sound generation
    void generate_pulse_wave(int channel, s16* buffer, size_t samples);
    void generate_triangle_wave(s16* buffer, size_t samples);
    void generate_noise_wave(s16* buffer, size_t samples);

    // Register helpers
    void update_pulse_channel(int channel, offs_t offset, u8 data);
    void update_triangle_channel(offs_t offset, u8 data);
    void update_noise_channel(offs_t offset, u8 data);
    void update_status_register(u8 data);

    // APU state
    struct pulse_channel {
        bool enabled = false;
        u8 duty_cycle = 0;
        u8 volume = 0;
        bool envelope_flag = false;
        u16 timer = 0;
        u8 length_counter = 0;

        // Audio generation state
        u32 phase_accumulator = 0;
        u32 frequency = 0;
    };

    struct triangle_channel {
        bool enabled = false;
        u8 linear_counter = 0;
        u16 timer = 0;
        u8 length_counter = 0;

        // Audio generation state
        u32 phase_accumulator = 0;
        u32 frequency = 0;
    };

    struct noise_channel {
        bool enabled = false;
        u8 volume = 0;
        u8 period = 0;
        bool mode_flag = false;  // 0 = long, 1 = short
        bool envelope_flag = false; // Envelope enable
        u8 length_counter = 0;

        // Audio generation state
        u16 shift_register = 1;
        u32 timer = 0;
    };

    pulse_channel m_pulse[2];
    triangle_channel m_triangle;
    noise_channel m_noise;

    u8 m_status_register = 0;
    u8 m_frame_counter = 0;

    u32 m_sample_rate = 44100;
    bool m_stream_enabled = false;

    // Callback handlers
    write8_delegate m_irq_handler;
    read8_delegate m_mem_read_cb;

    // Audio mixing
    static constexpr int PULSE_VOLUME_TABLE[16] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };

    static constexpr int TRIANGLE_WAVE[32] = {
        15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };

    static constexpr u8 PULSE_WAVE_TABLE[4][8] = {
        {0, 1, 0, 0, 0, 0, 0, 0},  // 12.5%
        {0, 1, 1, 0, 0, 0, 0, 0},  // 25%
        {0, 1, 1, 1, 1, 0, 0, 0},  // 50%
        {1, 0, 0, 1, 1, 1, 1, 1}   // 75%
    };

    static constexpr u16 NOISE_PERIOD_TABLE[16] = {
        4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
    };
};