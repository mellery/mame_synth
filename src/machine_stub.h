#pragma once

// Minimal MAME machine context stub for audio device initialization
// This provides the bare minimum infrastructure needed to instantiate MAME audio devices

#include <memory>
#include <string>

// For now, use void* to avoid incomplete type issues with unique_ptr
// Will be replaced with proper device_t* when we link MAME libraries
using audio_device_handle = void*;

class machine_stub {
public:
    machine_stub();
    ~machine_stub();

    // Initialize the minimal machine context
    bool initialize();
    void shutdown();

    // Check if machine is properly initialized
    bool is_initialized() const { return m_initialized; }

    // Get machine configuration info
    std::string get_config_info() const;

private:
    bool m_initialized = false;
    std::string m_machine_id;

    void setup_minimal_config();
};

// Simple device factory for creating MAME audio devices with our stub context
class audio_device_factory {
public:
    explicit audio_device_factory(machine_stub& machine);
    ~audio_device_factory();

    // Create NES APU device (returns handle for now)
    audio_device_handle create_nes_apu(const std::string& tag, uint32_t clock = 1789773);

    // Create SNES S-DSP device (returns handle for now)
    audio_device_handle create_snes_dsp(const std::string& tag, uint32_t clock = 32000);

    // Cleanup device handle
    void destroy_device(audio_device_handle handle);

private:
    machine_stub& m_machine;
};