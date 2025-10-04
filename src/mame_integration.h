#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <thread>
#include <atomic>

// Forward declarations to avoid heavy MAME headers in our interface
class nesapu_device;  // NES APU device
class sound_stream;
class machine_config;
class device_t;
class running_machine;
class osd_options;  // Changed from emu_options to osd_options
class osd_interface;
class machine_manager;

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

    // Audio callback interface - register a callback to receive mixed audio from MAME
    using audio_callback_t = std::function<void(const int16_t*, int)>;
    void set_audio_callback(audio_callback_t callback);

    // Internal - should not be used directly by client code
    minimal_machine_config* get_machine_config() { return m_machine_config; }
    machine_config* get_real_machine_config() { return m_real_machine_config.get(); }
    running_machine* get_running_machine() { return m_running_machine.get(); }
    osd_interface* get_osd_interface() { return m_osd; }
    void register_device(device_t* device);

private:
    bool m_initialized = false;
    minimal_machine_config* m_machine_config = nullptr; // Minimal MAME config wrapper

    // Full MAME runtime infrastructure
    osd_options* m_options = nullptr;                    // MAME OSD options (needed for osd_common_t)
    osd_interface* m_osd = nullptr;                      // OSD interface (can't use unique_ptr - protected destructor)
    machine_manager* m_manager = nullptr;                // Machine manager (can't use unique_ptr - incomplete type)
    std::unique_ptr<machine_config> m_real_machine_config;  // Real MAME machine_config
    std::unique_ptr<running_machine> m_running_machine;  // Real MAME running_machine

    std::vector<device_t*> m_devices;
    uint32_t m_sample_rate = 44100;

    // Helper to setup minimal machine config needed for audio devices
    bool setup_machine_config();
    void cleanup_machine_config();

    // Run MAME emulation loop with MIDI playback
    int run_with_midi(const std::vector<uint8_t> &midi_events, uint32_t duration_ms);
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
    virtual bool is_device_started() const = 0;  // Check if MAME device has been started

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
    bool is_device_started() const override;

    std::string get_device_name() const override { return "NES APU (MAME)"; }
    std::string get_device_tag() const override { return m_tag; }

    void write_register(uint32_t offset, uint8_t value) override;
    uint8_t read_register(uint32_t offset) const override;

    void update_audio_stream(int16_t* buffer, size_t sample_count) override;
    uint32_t get_sample_rate() const override;

    device_t* get_mame_device() override;

    // Warmup detection - returns true when MAME is generating audio
    bool is_warmed_up() const { return m_warmup_complete; }

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
    nesapu_device* m_apu = nullptr;      // NES APU device (owned by running_machine)
    void* m_sound_stream = nullptr;      // sound_stream* (opaque)
    std::vector<int16_t> m_audio_buffer; // Ring buffer for audio samples
    size_t m_buffer_write_pos = 0;       // Write position in ring buffer
    size_t m_buffer_read_pos = 0;        // Read position in ring buffer
    std::mutex m_buffer_mutex;           // Protect ring buffer access
    uint32_t m_sample_rate = 44100;
    bool m_warmup_complete = false;      // True when MAME starts generating non-zero audio

    // Internal helper for OSD audio callback
    void on_audio_callback(const int16_t* buffer, int sample_count);
    friend class mame_machine_context;  // Allow machine context to call on_audio_callback
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
    bool is_device_started() const override { return false; }  // Placeholder - not implemented yet

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