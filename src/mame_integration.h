#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

// Forward declarations to avoid heavy MAME headers in our interface
class nesapu_device;  // NES APU device
class sound_stream;
class machine_config;
class device_t;

// Include minimal MAME core for device integration
#include "mame_core/mame_minimal.h"

/**
 * MAME Integration Layer
 *
 * This provides a simplified interface to MAME's audio device system
 * without requiring full machine emulation setup.
 */

/**
 * Minimal machine context for MAME devices
 * Provides the minimal MAME environment needed for audio device operation
 */
class mame_machine_context {
public:
    mame_machine_context();
    ~mame_machine_context();

    // Initialize the minimal MAME environment needed for audio devices
    bool initialize();
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Get basic info
    std::string get_info() const;

    // Device creation interface (replaces factory pattern)
    std::unique_ptr<class mame_audio_device_base> create_nes_apu(
        const std::string& tag, uint32_t clock_rate);

    std::unique_ptr<class mame_audio_device_base> create_snes_dsp(
        const std::string& tag, uint32_t clock_rate);

    // Internal - should not be used directly by client code
    minimal_machine_config* get_machine_config() { return m_machine_config; }
    void register_device(device_t* device);

private:
    bool m_initialized = false;
    minimal_machine_config* m_machine_config = nullptr; // Real minimal MAME config
    std::vector<device_t*> m_devices;
    uint32_t m_sample_rate = 44100;

    // Helper to setup minimal machine config needed for audio devices
    bool setup_machine_config();
    void cleanup_machine_config();
};

/**
 * Base class for MAME audio device wrappers
 * This provides a common interface for all MAME audio devices
 */
class mame_audio_device_base {
public:
    virtual ~mame_audio_device_base() = default;

    // Device lifecycle
    virtual bool initialize() = 0;
    virtual void reset() = 0;
    virtual void shutdown() = 0;
    virtual bool is_initialized() const = 0;

    // Device identification
    virtual std::string get_device_name() const = 0;
    virtual std::string get_device_tag() const = 0;

    // Register interface - this is how we'll control the device
    virtual void write_register(uint32_t offset, uint8_t value) = 0;
    virtual uint8_t read_register(uint32_t offset) const = 0;

    // Audio output interface
    virtual void update_audio_stream(int16_t* buffer, size_t sample_count) = 0;
    virtual uint32_t get_sample_rate() const = 0;

    // Device-specific features can be accessed through derived classes
    virtual device_t* get_mame_device() = 0;

protected:
    mame_machine_context* m_machine_context = nullptr;
    std::string m_tag;
    uint32_t m_clock_rate = 0;
    bool m_initialized = false;
};

/**
 * NES APU MAME device wrapper
 */
class mame_nes_apu_device : public mame_audio_device_base {
public:
    mame_nes_apu_device(mame_machine_context* machine_ctx, const std::string& tag, uint32_t clock_rate);
    virtual ~mame_nes_apu_device();

    // mame_audio_device_base interface
    bool initialize() override;
    void reset() override;
    void shutdown() override;
    bool is_initialized() const override { return m_initialized; }

    std::string get_device_name() const override { return "NES APU (MAME)"; }
    std::string get_device_tag() const override { return m_tag; }

    void write_register(uint32_t offset, uint8_t value) override;
    uint8_t read_register(uint32_t offset) const override;

    void update_audio_stream(int16_t* buffer, size_t sample_count) override;
    uint32_t get_sample_rate() const override;

    device_t* get_mame_device() override;

    // NES APU specific interface
    nesapu_device* get_nes_apu_device();

    // Direct register access for debugging
    void write_pulse1_control(uint8_t value) { write_register(0x00, value); }
    void write_pulse1_sweep(uint8_t value) { write_register(0x01, value); }
    void write_pulse1_timer_low(uint8_t value) { write_register(0x02, value); }
    void write_pulse1_timer_high(uint8_t value) { write_register(0x03, value); }

    void write_pulse2_control(uint8_t value) { write_register(0x04, value); }
    void write_pulse2_sweep(uint8_t value) { write_register(0x05, value); }
    void write_pulse2_timer_low(uint8_t value) { write_register(0x06, value); }
    void write_pulse2_timer_high(uint8_t value) { write_register(0x07, value); }

    void write_triangle_linear(uint8_t value) { write_register(0x08, value); }
    void write_triangle_timer_low(uint8_t value) { write_register(0x0A, value); }
    void write_triangle_timer_high(uint8_t value) { write_register(0x0B, value); }

    void write_noise_envelope(uint8_t value) { write_register(0x0C, value); }
    void write_noise_period(uint8_t value) { write_register(0x0E, value); }
    void write_noise_length(uint8_t value) { write_register(0x0F, value); }

    void write_status_register(uint8_t value) { write_register(0x15, value); }
    uint8_t read_status_register() const { return read_register(0x15); }

private:
    nesapu_device* m_apu = nullptr;      // NES APU device
    void* m_sound_stream = nullptr;      // sound_stream* (opaque)
    std::vector<int16_t> m_audio_buffer;
    uint32_t m_sample_rate = 44100;
};

/**
 * SNES S-DSP MAME device wrapper (placeholder for future implementation)
 */
class mame_snes_dsp_device : public mame_audio_device_base {
public:
    mame_snes_dsp_device(mame_machine_context* machine_ctx, const std::string& tag, uint32_t clock_rate);
    virtual ~mame_snes_dsp_device();

    // mame_audio_device_base interface
    bool initialize() override;
    void reset() override;
    void shutdown() override;
    bool is_initialized() const override { return m_initialized; }

    std::string get_device_name() const override { return "SNES S-DSP (MAME - Placeholder)"; }
    std::string get_device_tag() const override { return m_tag; }

    void write_register(uint32_t offset, uint8_t value) override;
    uint8_t read_register(uint32_t offset) const override;

    void update_audio_stream(int16_t* buffer, size_t sample_count) override;
    uint32_t get_sample_rate() const override;

    device_t* get_mame_device() override;

private:
    device_t* m_snes_device = nullptr; // Placeholder - will implement when we find the right MAME device
    std::vector<uint8_t> m_registers; // Simulate register storage for now
};

/**
 * Factory for creating MAME-integrated audio devices
 * This replaces our handle-based audio_device_factory
 */
class mame_device_factory {
public:
    explicit mame_device_factory(mame_machine_context* machine_ctx);

    std::unique_ptr<mame_audio_device_base> create_nes_apu(
        const std::string& tag, uint32_t clock_rate = 1789773);

    std::unique_ptr<mame_audio_device_base> create_snes_dsp(
        const std::string& tag, uint32_t clock_rate = 24576000);

    // Get list of supported device types
    std::vector<std::string> get_supported_devices() const;

private:
    mame_machine_context* m_machine_context;
};