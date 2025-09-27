#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <array>

// Forward declarations
class nes_apu_device;
class audio_device_manager;

/**
 * NES-focused audio mixer that implements hardware-accurate NES APU mixing
 *
 * The NES APU has specific mixing characteristics that differ from simple linear mixing:
 * - Non-linear DAC conversion for pulse and triangle channels
 * - Different mixing formulas for different channel groups
 * - Hardware-accurate high-pass filtering
 * - Channel-specific volume curves and interactions
 */
class nes_audio_mixer {
public:
    struct nes_channel_sample {
        int16_t pulse1 = 0;
        int16_t pulse2 = 0;
        int16_t triangle = 0;
        int16_t noise = 0;
        int16_t dmc = 0;
    };

    struct nes_mixer_config {
        uint32_t sample_rate;
        bool enable_nonlinear_mixing;  // Use hardware-accurate non-linear mixing
        bool enable_highpass_filter;   // Apply 90Hz high-pass filter like real NES
        bool enable_lowpass_filter;    // Apply 14kHz low-pass filter
        float pulse_volume_scale;      // Scale factor for pulse channels
        float triangle_volume_scale;   // Scale factor for triangle channel
        float noise_volume_scale;      // Scale factor for noise channel
        float dmc_volume_scale;        // Scale factor for DMC channel

        nes_mixer_config()
            : sample_rate(44100)
            , enable_nonlinear_mixing(true)
            , enable_highpass_filter(true)
            , enable_lowpass_filter(true)
            , pulse_volume_scale(1.0f)
            , triangle_volume_scale(1.0f)
            , noise_volume_scale(1.0f)
            , dmc_volume_scale(1.0f) {}
    };

    explicit nes_audio_mixer(const nes_mixer_config& config = nes_mixer_config{});
    ~nes_audio_mixer();

    // Mixer lifecycle
    bool initialize(uint32_t sample_rate);
    void reset();
    void shutdown();

    // Configuration
    void set_config(const nes_mixer_config& config);
    nes_mixer_config get_config() const { return m_config; }

    // Sample generation - takes individual channel samples and produces mixed output
    void mix_nes_samples(const nes_channel_sample* channel_samples, int16_t* output, size_t sample_count);

    // Integrated mixing for NES APU devices
    void mix_nes_devices(const std::vector<nes_apu_device*>& devices, int16_t* output, size_t sample_count);

    // Hardware-accurate mixing functions
    static float pulse_dac_output(uint8_t pulse1, uint8_t pulse2);
    static float tnd_dac_output(uint8_t triangle, uint8_t noise, uint8_t dmc);

    // Channel volume and panning
    void set_channel_volume(uint8_t channel, float volume);
    void set_channel_pan(uint8_t channel, float pan); // -1.0 (left) to 1.0 (right)
    void set_master_volume(float volume);

    // Performance monitoring
    struct mixer_stats {
        uint64_t samples_mixed = 0;
        uint64_t buffer_underruns = 0;
        double cpu_usage = 0.0;
        float peak_output_level = 0.0f;
    };
    mixer_stats get_stats() const;

private:
    nes_mixer_config m_config;
    bool m_initialized = false;

    // Per-channel mixing state
    struct channel_state {
        float volume = 1.0f;
        float pan = 0.0f;  // 0.0 = center
        bool muted = false;
    };
    std::array<channel_state, 5> m_channels; // Pulse1, Pulse2, Triangle, Noise, DMC

    float m_master_volume = 1.0f;

    // High-pass filter state (90Hz like real NES)
    struct highpass_filter {
        float x_prev = 0.0f;
        float y_prev = 0.0f;
        float alpha = 0.0f;  // Filter coefficient
    };
    highpass_filter m_hp_filter;

    // Low-pass filter state (14kHz)
    struct lowpass_filter {
        float y_prev = 0.0f;
        float alpha = 0.0f;  // Filter coefficient
    };
    lowpass_filter m_lp_filter;

    // Performance tracking
    mutable mixer_stats m_stats;

    // Internal mixing functions
    float apply_nonlinear_mixing(const nes_channel_sample& sample) const;
    float apply_linear_mixing(const nes_channel_sample& sample) const;
    float apply_highpass_filter(float input);
    float apply_lowpass_filter(float input);
    void update_filter_coefficients();

    // Sample processing pipeline
    void process_sample_pipeline(const nes_channel_sample& input, int16_t& output);

    // Statistics tracking
    void update_stats(size_t sample_count, float peak_level);
};

/**
 * Enhanced audio device manager with NES-focused mixing capabilities
 */
class nes_enhanced_audio_manager {
public:
    nes_enhanced_audio_manager();
    ~nes_enhanced_audio_manager();

    // Device management (delegates to base manager)
    bool add_device(std::unique_ptr<class audio_device> device);
    class audio_device* get_device(const std::string& name);
    std::vector<std::string> get_device_names() const;
    void remove_device(const std::string& name);

    // Enhanced initialization with NES mixer
    bool initialize(uint32_t sample_rate, const nes_audio_mixer::nes_mixer_config& mixer_config = nes_audio_mixer::nes_mixer_config{});
    void reset();
    void shutdown();

    // Enhanced sample generation with NES-focused mixing
    void generate_enhanced_samples(int16_t* buffer, size_t sample_count);

    // NES mixer access
    nes_audio_mixer* get_nes_mixer() { return m_nes_mixer.get(); }
    const nes_audio_mixer* get_nes_mixer() const { return m_nes_mixer.get(); }

    // Configuration
    void set_mixing_mode(bool nes_focused_mode);
    bool is_nes_focused_mode() const { return m_nes_focused_mode; }

    // Statistics
    struct enhanced_stats {
        nes_audio_mixer::mixer_stats nes_mixer_stats;
        uint64_t total_samples_generated = 0;
        uint32_t active_nes_devices = 0;
        uint32_t active_other_devices = 0;
    };
    enhanced_stats get_enhanced_stats() const;

private:
    std::unique_ptr<audio_device_manager> m_base_manager;
    std::unique_ptr<nes_audio_mixer> m_nes_mixer;
    bool m_nes_focused_mode = true;
    bool m_initialized = false;

    // Separate NES devices for optimized mixing
    std::vector<nes_apu_device*> m_nes_devices;
    std::vector<class audio_device*> m_other_devices;

    // Temporary buffers for mixing
    std::vector<nes_audio_mixer::nes_channel_sample> m_nes_sample_buffer;
    std::vector<int16_t> m_temp_buffer;

    // Internal helper functions
    void update_device_lists();
    void mix_nes_devices_optimized(int16_t* buffer, size_t sample_count);
    void mix_other_devices_standard(int16_t* buffer, size_t sample_count);
};