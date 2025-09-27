#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <memory>

// Forward declarations
struct music_note;
struct music_control;
struct music_program;

/**
 * Base class for device register mapping
 * Handles conversion of musical data to device-specific register values
 */
class register_mapper {
public:
    virtual ~register_mapper() = default;

    // Core mapping interface
    virtual bool map_note_on(const music_note& note, std::vector<uint8_t>& registers) = 0;
    virtual bool map_note_off(uint8_t channel, uint8_t note_number, std::vector<uint8_t>& registers) = 0;
    virtual bool map_program_change(const music_program& program, std::vector<uint8_t>& registers) = 0;
    virtual bool map_control_change(const music_control& control, std::vector<uint8_t>& registers) = 0;

    // Device information
    virtual uint32_t get_register_count() const = 0;
    virtual std::string get_device_name() const = 0;
    virtual uint32_t get_base_clock_rate() const = 0;

    // Register access helpers
    virtual uint8_t read_register(uint32_t address) const = 0;
    virtual void write_register(uint32_t address, uint8_t value) = 0;
    virtual void reset_registers() = 0;

protected:
    // Common helper functions
    static double midi_note_to_frequency(uint8_t note_number);
    static uint8_t clamp_to_range(int32_t value, int32_t min, int32_t max);
};

/**
 * NES APU Register Mapper
 * Maps musical data to NES APU register format
 *
 * NES APU Memory Map:
 * $4000-$4003: Pulse 1
 * $4004-$4007: Pulse 2
 * $4008-$400B: Triangle
 * $400C-$400F: Noise
 * $4010-$4013: DMC
 * $4015: Channel enable/disable
 * $4017: Frame counter
 */
class nes_apu_mapper : public register_mapper {
public:
    static constexpr uint32_t REGISTER_COUNT = 0x18; // $4000-$4017
    static constexpr uint32_t BASE_CLOCK_NTSC = 1789773;
    static constexpr uint32_t BASE_CLOCK_PAL = 1662607;

    explicit nes_apu_mapper(uint32_t clock_rate = BASE_CLOCK_NTSC);

    // register_mapper interface
    bool map_note_on(const music_note& note, std::vector<uint8_t>& registers) override;
    bool map_note_off(uint8_t channel, uint8_t note_number, std::vector<uint8_t>& registers) override;
    bool map_program_change(const music_program& program, std::vector<uint8_t>& registers) override;
    bool map_control_change(const music_control& control, std::vector<uint8_t>& registers) override;

    uint32_t get_register_count() const override { return REGISTER_COUNT; }
    std::string get_device_name() const override { return "NES APU"; }
    uint32_t get_base_clock_rate() const override { return m_clock_rate; }

    uint8_t read_register(uint32_t address) const override;
    void write_register(uint32_t address, uint8_t value) override;
    void reset_registers() override;

    // NES APU specific functionality
    void set_pulse_duty_cycle(uint8_t channel, uint8_t duty); // 0-3
    void set_pulse_envelope(uint8_t channel, bool constant_volume, uint8_t volume);
    void set_triangle_linear_counter(uint8_t counter_load);
    void set_noise_period(uint16_t period, bool short_mode);
    void set_channel_enable(uint8_t channel, bool enable);

    // Register structure helpers
    struct pulse_registers {
        uint8_t control;     // $4000/$4004 - Duty, Loop, Constant, Volume
        uint8_t sweep;       // $4001/$4005 - Sweep unit
        uint8_t timer_low;   // $4002/$4006 - Timer low 8 bits
        uint8_t timer_high;  // $4003/$4007 - Length counter load, Timer high 3 bits
    };

    struct triangle_registers {
        uint8_t linear_counter; // $4008 - Linear counter control
        uint8_t unused;         // $4009 - Unused
        uint8_t timer_low;      // $400A - Timer low 8 bits
        uint8_t timer_high;     // $400B - Length counter load, Timer high 3 bits
    };

    struct noise_registers {
        uint8_t envelope;    // $400C - Envelope control
        uint8_t unused;      // $400D - Unused
        uint8_t period;      // $400E - Period and waveform
        uint8_t length;      // $400F - Length counter load
    };

private:
    uint32_t m_clock_rate;
    std::array<uint8_t, REGISTER_COUNT> m_registers;

    // Channel state tracking
    struct channel_state {
        bool active = false;
        uint8_t note_number = 0;
        uint8_t velocity = 0;
        uint16_t timer_value = 0;
        uint8_t duty_cycle = 2; // Default 50%
    };

    std::array<channel_state, 5> m_channels; // Pulse1, Pulse2, Triangle, Noise, DMC

    // Note-to-timer conversion
    uint16_t note_to_timer_value(uint8_t note_number) const;
    uint8_t velocity_to_volume(uint8_t velocity) const;

    // Register address helpers
    uint32_t get_channel_base_address(uint8_t channel) const;
    bool is_valid_address(uint32_t address) const;
};

/**
 * SNES S-DSP Register Mapper
 * Maps musical data to SNES S-DSP register format
 *
 * SNES S-DSP Register Map:
 * Each voice has 16 bytes of registers (0x00-0x0F offset per voice)
 * Voice 0: $00-$0F, Voice 1: $10-$1F, ..., Voice 7: $70-$7F
 * Global registers: $0C, $1C, $2C, $3C, $4C, $5C, $6C, $7C (shared)
 */
class snes_dsp_mapper : public register_mapper {
public:
    static constexpr uint32_t REGISTER_COUNT = 0x80; // 128 registers
    static constexpr uint32_t VOICE_COUNT = 8;
    static constexpr uint32_t BASE_CLOCK = 24576000; // 24.576 MHz

    explicit snes_dsp_mapper(uint32_t clock_rate = BASE_CLOCK);

    // register_mapper interface
    bool map_note_on(const music_note& note, std::vector<uint8_t>& registers) override;
    bool map_note_off(uint8_t channel, uint8_t note_number, std::vector<uint8_t>& registers) override;
    bool map_program_change(const music_program& program, std::vector<uint8_t>& registers) override;
    bool map_control_change(const music_control& control, std::vector<uint8_t>& registers) override;

    uint32_t get_register_count() const override { return REGISTER_COUNT; }
    std::string get_device_name() const override { return "SNES S-DSP"; }
    uint32_t get_base_clock_rate() const override { return m_clock_rate; }

    uint8_t read_register(uint32_t address) const override;
    void write_register(uint32_t address, uint8_t value) override;
    void reset_registers() override;

    // SNES S-DSP specific functionality
    void set_voice_volume(uint8_t voice, uint8_t left_vol, uint8_t right_vol);
    void set_voice_pitch(uint8_t voice, uint16_t pitch);
    void set_voice_source(uint8_t voice, uint8_t source_number);
    void set_voice_adsr(uint8_t voice, uint16_t adsr);
    void set_master_volume(uint8_t left_vol, uint8_t right_vol);
    void set_echo_volume(uint8_t left_vol, uint8_t right_vol);
    void set_key_on(uint8_t voice_mask);
    void set_key_off(uint8_t voice_mask);

    // Voice register offsets
    enum voice_register_offset {
        VOICE_LEFT_VOLUME = 0x00,
        VOICE_RIGHT_VOLUME = 0x01,
        VOICE_PITCH_LOW = 0x02,
        VOICE_PITCH_HIGH = 0x03,
        VOICE_SOURCE_NUMBER = 0x04,
        VOICE_ADSR_LOW = 0x05,
        VOICE_ADSR_HIGH = 0x06,
        VOICE_GAIN = 0x07,
        VOICE_ENVX = 0x08,
        VOICE_OUTX = 0x09
    };

    // Global register addresses
    enum global_register {
        MASTER_LEFT_VOLUME = 0x0C,
        MASTER_RIGHT_VOLUME = 0x1C,
        ECHO_LEFT_VOLUME = 0x2C,
        ECHO_RIGHT_VOLUME = 0x3C,
        KEY_ON = 0x4C,
        KEY_OFF = 0x5C,
        SOURCE_FLAGS = 0x6C,
        ECHO_FEEDBACK = 0x0D,
        PITCH_MOD_FLAGS = 0x2D,
        NOISE_FLAGS = 0x3D,
        ECHO_FLAGS = 0x4D,
        SAMPLE_TABLE_ADDR = 0x5D,
        ECHO_RING_ADDR = 0x6D,
        ECHO_RING_SIZE = 0x7D
    };

private:
    uint32_t m_clock_rate;
    std::array<uint8_t, REGISTER_COUNT> m_registers;

    // Voice state tracking
    struct voice_state {
        bool active = false;
        uint8_t note_number = 0;
        uint8_t velocity = 0;
        uint16_t pitch = 0x1000; // Default pitch (no change)
        uint8_t source_number = 0;
        uint8_t left_volume = 0x7F;
        uint8_t right_volume = 0x7F;
    };

    std::array<voice_state, VOICE_COUNT> m_voices;

    // Note-to-pitch conversion
    uint16_t note_to_pitch_value(uint8_t note_number) const;
    uint8_t velocity_to_volume(uint8_t velocity) const;

    // Register address helpers
    uint32_t get_voice_register_address(uint8_t voice, voice_register_offset offset) const;
    bool is_valid_address(uint32_t address) const;
    bool is_voice_register(uint32_t address) const;
    uint8_t get_voice_from_address(uint32_t address) const;
};

/**
 * Register mapping factory
 * Creates appropriate register mappers for different device types
 */
class register_mapping_factory {
public:
    enum device_type {
        NES_APU,
        SNES_DSP,
        // Future: GB_AUDIO, YM2612, etc.
    };

    static std::unique_ptr<register_mapper> create_mapper(device_type type, uint32_t clock_rate = 0);
    static std::vector<device_type> supported_devices();
    static std::string device_type_name(device_type type);
};

/**
 * Register snapshot for debugging and analysis
 */
struct register_snapshot {
    std::string device_name;
    uint32_t timestamp_ms;
    std::vector<uint8_t> register_data;
    std::string description;

    register_snapshot(const std::string& name, uint32_t timestamp,
                     const std::vector<uint8_t>& data, const std::string& desc = "")
        : device_name(name), timestamp_ms(timestamp), register_data(data), description(desc) {}
};

/**
 * Register diff utility for comparing register states
 */
class register_diff {
public:
    struct change {
        uint32_t address;
        uint8_t old_value;
        uint8_t new_value;
    };

    static std::vector<change> compare(const register_snapshot& before,
                                     const register_snapshot& after);
    static std::string format_changes(const std::vector<change>& changes);
    static void print_changes(const std::vector<change>& changes, const std::string& device_name);
};