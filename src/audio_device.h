#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

// Forward declarations
struct music_note;
struct music_control;
struct music_program;

namespace nes_note_mapping {
    class nes_note_mapper;
}

/**
 * Abstract base class for audio device abstraction
 * Provides common interface for all audio devices (NES APU, SNES S-DSP, etc.)
 */
class audio_device {
public:
    virtual ~audio_device() = default;

    // Device lifecycle
    virtual bool initialize(uint32_t sample_rate) = 0;
    virtual void reset() = 0;
    virtual void shutdown() = 0;

    // Device information
    virtual std::string get_name() const = 0;
    virtual std::string get_description() const = 0;
    virtual uint32_t get_sample_rate() const = 0;
    virtual uint32_t get_channel_count() const = 0;

    // Music playback interface
    virtual bool play_note(const music_note& note) = 0;
    virtual bool stop_note(uint8_t channel, uint8_t note_number) = 0;
    virtual bool set_program(const music_program& program) = 0;
    virtual bool set_control(const music_control& control) = 0;

    // Audio generation
    virtual void generate_samples(int16_t* buffer, size_t sample_count) = 0;
    virtual bool is_playing() const = 0;

    // Channel management
    virtual void set_channel_volume(uint8_t channel, uint8_t volume) = 0;
    virtual void set_master_volume(uint8_t volume) = 0;
    virtual void mute_channel(uint8_t channel, bool mute) = 0;

protected:
    uint32_t m_sample_rate = 44100;
    bool m_initialized = false;
};

/**
 * NES APU (Audio Processing Unit) device abstraction
 * Handles 2 pulse channels, 1 triangle, 1 noise, and 1 DMC channel
 */
class nes_apu_device : public audio_device {
public:
    explicit nes_apu_device(const std::string& tag, uint32_t clock_rate);
    virtual ~nes_apu_device();

    // audio_device interface
    bool initialize(uint32_t sample_rate) override;
    void reset() override;
    void shutdown() override;

    std::string get_name() const override { return "NES APU"; }
    std::string get_description() const override { return "Nintendo Entertainment System Audio Processing Unit"; }
    uint32_t get_sample_rate() const override { return m_sample_rate; }
    uint32_t get_channel_count() const override { return 5; } // 2 pulse + triangle + noise + DMC

    bool play_note(const music_note& note) override;
    bool stop_note(uint8_t channel, uint8_t note_number) override;
    bool set_program(const music_program& program) override;
    bool set_control(const music_control& control) override;

    void generate_samples(int16_t* buffer, size_t sample_count) override;
    bool is_playing() const override;

    void set_channel_volume(uint8_t channel, uint8_t volume) override;
    void set_master_volume(uint8_t volume) override;
    void mute_channel(uint8_t channel, bool mute) override;

    // NES-specific functionality
    void set_pulse_duty_cycle(uint8_t channel, uint8_t duty); // 0-3 (12.5%, 25%, 50%, 25% negated)
    void set_triangle_linear_counter(uint8_t value);
    void set_noise_mode(bool short_mode); // true for short (93-bit), false for long (32767-bit)

private:
    struct channel_state {
        bool active = false;
        bool muted = false;
        uint8_t volume = 0x0F;
        uint16_t frequency = 0;
        uint8_t note_number = 0;
        uint8_t velocity = 0;
    };

    std::string m_tag;
    uint32_t m_clock_rate;
    channel_state m_channels[5]; // Pulse1, Pulse2, Triangle, Noise, DMC
    uint8_t m_master_volume = 0xFF;

    // NES APU register simulation (kept for compatibility)
    uint8_t m_pulse_duty[2] = {0, 0}; // Duty cycle for pulse channels
    uint8_t m_triangle_linear = 0;
    bool m_noise_short_mode = false;

    // MAME integration backend
    std::unique_ptr<class mame_machine_context> m_mame_context;
    std::unique_ptr<class mame_audio_device_base> m_mame_device;

    // NES note mapping system
    std::unique_ptr<class nes_note_mapping::nes_note_mapper> m_note_mapper;

    // Helper functions
    uint16_t note_to_nes_frequency(uint8_t note_number) const;
    uint8_t map_velocity_to_volume(uint8_t velocity) const;
    void sync_to_mame_device(); // Synchronize state with MAME device
};

/**
 * SNES S-DSP (Sony Digital Signal Processor) device abstraction
 * Handles 8 channels with sample-based synthesis
 */
class snes_dsp_device : public audio_device {
public:
    explicit snes_dsp_device(const std::string& tag, uint32_t clock_rate);
    virtual ~snes_dsp_device();

    // audio_device interface
    bool initialize(uint32_t sample_rate) override;
    void reset() override;
    void shutdown() override;

    std::string get_name() const override { return "SNES S-DSP"; }
    std::string get_description() const override { return "Super Nintendo S-DSP Sound Processor"; }
    uint32_t get_sample_rate() const override { return m_sample_rate; }
    uint32_t get_channel_count() const override { return 8; }

    bool play_note(const music_note& note) override;
    bool stop_note(uint8_t channel, uint8_t note_number) override;
    bool set_program(const music_program& program) override;
    bool set_control(const music_control& control) override;

    void generate_samples(int16_t* buffer, size_t sample_count) override;
    bool is_playing() const override;

    void set_channel_volume(uint8_t channel, uint8_t volume) override;
    void set_master_volume(uint8_t volume) override;
    void mute_channel(uint8_t channel, bool mute) override;

    // SNES-specific functionality
    void set_echo_enable(uint8_t channel, bool enable);
    void set_noise_enable(uint8_t channel, bool enable);
    void set_pitch_modulation(uint8_t channel, bool enable);

private:
    struct voice_state {
        bool active = false;
        bool muted = false;
        bool echo_enable = false;
        bool noise_enable = false;
        bool pitch_mod_enable = false;
        uint8_t volume_left = 0x7F;
        uint8_t volume_right = 0x7F;
        uint16_t pitch = 0x1000;
        uint8_t source_number = 0; // Sample/instrument number
        uint8_t note_number = 0;
        uint8_t velocity = 0;
    };

    std::string m_tag;
    uint32_t m_clock_rate;
    voice_state m_voices[8];
    uint8_t m_master_volume_left = 0x7F;
    uint8_t m_master_volume_right = 0x7F;

    // S-DSP register simulation
    uint8_t m_echo_volume_left = 0;
    uint8_t m_echo_volume_right = 0;
    uint8_t m_noise_clock = 0;

    // Note-to-pitch conversion for SNES S-DSP
    uint16_t note_to_snes_pitch(uint8_t note_number) const;
    uint8_t map_velocity_to_volume(uint8_t velocity) const;
};

/**
 * Audio device manager - manages multiple audio devices and routing
 */
class audio_device_manager {
public:
    audio_device_manager() = default;
    ~audio_device_manager();

    // Device management
    bool add_device(std::unique_ptr<audio_device> device);
    audio_device* get_device(const std::string& name);
    std::vector<std::string> get_device_names() const;
    void remove_device(const std::string& name);
    void clear_devices();

    // Global operations
    bool initialize_all(uint32_t sample_rate);
    void reset_all();
    void shutdown_all();

    // Music playback coordination
    bool play_note_on_device(const std::string& device_name, const music_note& note);
    bool stop_note_on_device(const std::string& device_name, uint8_t channel, uint8_t note_number);
    bool set_program_on_device(const std::string& device_name, const music_program& program);
    bool set_control_on_device(const std::string& device_name, const music_control& control);

    // Mixed audio output
    void generate_mixed_samples(int16_t* buffer, size_t sample_count);
    bool any_device_playing() const;

    // Global volume control
    void set_global_volume(uint8_t volume);
    uint8_t get_global_volume() const { return m_global_volume; }

private:
    std::vector<std::unique_ptr<audio_device>> m_devices;
    uint8_t m_global_volume = 0xFF;
    uint32_t m_sample_rate = 44100;
    bool m_initialized = false;

    // Temporary buffer for mixing
    std::vector<int32_t> m_mix_buffer;
};