#include "nes_audio_mixer.h"
#include "audio_device.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>

// NES APU hardware mixing constants
static constexpr float NES_PULSE_MAX_OUTPUT = 95.88f;
static constexpr float NES_TND_MAX_OUTPUT = 159.79f;

// Filter constants
static constexpr float PI = 3.14159265359f;

nes_audio_mixer::nes_audio_mixer(const nes_mixer_config& config)
    : m_config(config) {
    // Initialize channel states
    for (auto& channel : m_channels) {
        channel.volume = 1.0f;
        channel.pan = 0.0f;
        channel.muted = false;
    }
}

nes_audio_mixer::~nes_audio_mixer() {
    shutdown();
}

bool nes_audio_mixer::initialize(uint32_t sample_rate) {
    if (m_initialized) {
        return true;
    }

    m_config.sample_rate = sample_rate;
    update_filter_coefficients();

    // Reset filter states
    m_hp_filter = {};
    m_lp_filter = {};

    m_initialized = true;
    std::cout << "NES audio mixer initialized at " << sample_rate << "Hz" << std::endl;
    return true;
}

void nes_audio_mixer::reset() {
    // Reset filter states
    m_hp_filter = {};
    m_lp_filter = {};

    // Reset statistics
    m_stats = {};

    std::cout << "NES audio mixer reset" << std::endl;
}

void nes_audio_mixer::shutdown() {
    if (m_initialized) {
        m_initialized = false;
        std::cout << "NES audio mixer shut down" << std::endl;
    }
}

void nes_audio_mixer::set_config(const nes_mixer_config& config) {
    m_config = config;
    if (m_initialized) {
        update_filter_coefficients();
    }
}

void nes_audio_mixer::mix_nes_samples(const nes_channel_sample* channel_samples, int16_t* output, size_t sample_count) {
    if (!m_initialized || !channel_samples || !output) {
        return;
    }

    float peak_level = 0.0f;

    for (size_t i = 0; i < sample_count; ++i) {
        process_sample_pipeline(channel_samples[i], output[i]);

        // Track peak level for statistics
        float abs_sample = std::abs(static_cast<float>(output[i]) / 32767.0f);
        peak_level = std::max(peak_level, abs_sample);
    }

    update_stats(sample_count, peak_level);
}

void nes_audio_mixer::mix_nes_devices(const std::vector<nes_apu_device*>& devices, int16_t* output, size_t sample_count) {
    if (!m_initialized || devices.empty()) {
        std::memset(output, 0, sample_count * sizeof(int16_t));
        return;
    }

    // For now, simplified implementation - mix first NES device
    // In a full implementation, we would need to extract individual channel data
    // from multiple NES devices and mix them properly
    if (!devices.empty()) {
        devices[0]->generate_samples(output, sample_count);

        // Apply master volume
        for (size_t i = 0; i < sample_count; ++i) {
            float sample = static_cast<float>(output[i]) * m_master_volume;

            // Apply filters if enabled
            if (m_config.enable_highpass_filter) {
                sample = apply_highpass_filter(sample);
            }
            if (m_config.enable_lowpass_filter) {
                sample = apply_lowpass_filter(sample);
            }

            // Clip and convert back
            if (sample > 32767.0f) sample = 32767.0f;
            else if (sample < -32768.0f) sample = -32768.0f;
            output[i] = static_cast<int16_t>(sample);
        }
    }
}

// Hardware-accurate NES APU DAC output functions
float nes_audio_mixer::pulse_dac_output(uint8_t pulse1, uint8_t pulse2) {
    if (pulse1 == 0 && pulse2 == 0) {
        return 0.0f;
    }
    return NES_PULSE_MAX_OUTPUT / ((8128.0f / (pulse1 + pulse2)) + 100.0f);
}

float nes_audio_mixer::tnd_dac_output(uint8_t triangle, uint8_t noise, uint8_t dmc) {
    if (triangle == 0 && noise == 0 && dmc == 0) {
        return 0.0f;
    }
    return NES_TND_MAX_OUTPUT / ((1.0f / ((triangle / 8227.0f) + (noise / 12241.0f) + (dmc / 22638.0f))) + 100.0f);
}

void nes_audio_mixer::set_channel_volume(uint8_t channel, float volume) {
    if (channel < m_channels.size()) {
        m_channels[channel].volume = std::clamp(volume, 0.0f, 2.0f);
    }
}

void nes_audio_mixer::set_channel_pan(uint8_t channel, float pan) {
    if (channel < m_channels.size()) {
        m_channels[channel].pan = std::clamp(pan, -1.0f, 1.0f);
    }
}

void nes_audio_mixer::set_master_volume(float volume) {
    m_master_volume = std::clamp(volume, 0.0f, 2.0f);
}

nes_audio_mixer::mixer_stats nes_audio_mixer::get_stats() const {
    return m_stats;
}

float nes_audio_mixer::apply_nonlinear_mixing(const nes_channel_sample& sample) const {
    // Apply channel volume scaling
    uint8_t pulse1 = static_cast<uint8_t>(std::clamp(
        static_cast<int>(sample.pulse1 * m_config.pulse_volume_scale * m_channels[0].volume / 1000.0f), 0, 15));
    uint8_t pulse2 = static_cast<uint8_t>(std::clamp(
        static_cast<int>(sample.pulse2 * m_config.pulse_volume_scale * m_channels[1].volume / 1000.0f), 0, 15));
    uint8_t triangle = static_cast<uint8_t>(std::clamp(
        static_cast<int>(sample.triangle * m_config.triangle_volume_scale * m_channels[2].volume / 1000.0f), 0, 15));
    uint8_t noise = static_cast<uint8_t>(std::clamp(
        static_cast<int>(sample.noise * m_config.noise_volume_scale * m_channels[3].volume / 1000.0f), 0, 15));
    uint8_t dmc = static_cast<uint8_t>(std::clamp(
        static_cast<int>(sample.dmc * m_config.dmc_volume_scale * m_channels[4].volume / 1000.0f), 0, 127));

    // Use hardware-accurate DAC mixing
    float pulse_out = pulse_dac_output(pulse1, pulse2);
    float tnd_out = tnd_dac_output(triangle, noise, dmc);

    return (pulse_out + tnd_out) * m_master_volume;
}

float nes_audio_mixer::apply_linear_mixing(const nes_channel_sample& sample) const {
    float mixed = 0.0f;

    // Simple linear mixing with per-channel volume
    mixed += sample.pulse1 * m_config.pulse_volume_scale * m_channels[0].volume;
    mixed += sample.pulse2 * m_config.pulse_volume_scale * m_channels[1].volume;
    mixed += sample.triangle * m_config.triangle_volume_scale * m_channels[2].volume;
    mixed += sample.noise * m_config.noise_volume_scale * m_channels[3].volume;
    mixed += sample.dmc * m_config.dmc_volume_scale * m_channels[4].volume;

    return mixed * m_master_volume / 5.0f; // Average the channels
}

float nes_audio_mixer::apply_highpass_filter(float input) {
    // First-order high-pass filter (90Hz cutoff like real NES)
    float output = m_hp_filter.alpha * (m_hp_filter.y_prev + input - m_hp_filter.x_prev);
    m_hp_filter.x_prev = input;
    m_hp_filter.y_prev = output;
    return output;
}

float nes_audio_mixer::apply_lowpass_filter(float input) {
    // First-order low-pass filter (14kHz cutoff)
    float output = m_lp_filter.y_prev + m_lp_filter.alpha * (input - m_lp_filter.y_prev);
    m_lp_filter.y_prev = output;
    return output;
}

void nes_audio_mixer::update_filter_coefficients() {
    if (m_config.sample_rate == 0) return;

    // High-pass filter (90Hz cutoff)
    float hp_rc = 1.0f / (2.0f * PI * 90.0f);
    float hp_dt = 1.0f / static_cast<float>(m_config.sample_rate);
    m_hp_filter.alpha = hp_rc / (hp_rc + hp_dt);

    // Low-pass filter (14kHz cutoff)
    float lp_rc = 1.0f / (2.0f * PI * 14000.0f);
    float lp_dt = 1.0f / static_cast<float>(m_config.sample_rate);
    m_lp_filter.alpha = lp_dt / (lp_rc + lp_dt);
}

void nes_audio_mixer::process_sample_pipeline(const nes_channel_sample& input, int16_t& output) {
    float mixed_sample;

    // Apply mixing (linear or non-linear)
    if (m_config.enable_nonlinear_mixing) {
        mixed_sample = apply_nonlinear_mixing(input);
    } else {
        mixed_sample = apply_linear_mixing(input);
    }

    // Apply filters
    if (m_config.enable_highpass_filter) {
        mixed_sample = apply_highpass_filter(mixed_sample);
    }
    if (m_config.enable_lowpass_filter) {
        mixed_sample = apply_lowpass_filter(mixed_sample);
    }

    // Clip and convert to 16-bit
    if (mixed_sample > 32767.0f) mixed_sample = 32767.0f;
    else if (mixed_sample < -32768.0f) mixed_sample = -32768.0f;

    output = static_cast<int16_t>(mixed_sample);
}

void nes_audio_mixer::update_stats(size_t sample_count, float peak_level) {
    m_stats.samples_mixed += sample_count;
    m_stats.peak_output_level = std::max(m_stats.peak_output_level, peak_level);
}

// Enhanced Audio Device Manager Implementation

nes_enhanced_audio_manager::nes_enhanced_audio_manager()
    : m_base_manager(std::make_unique<audio_device_manager>())
    , m_nes_mixer(std::make_unique<nes_audio_mixer>()) {
}

nes_enhanced_audio_manager::~nes_enhanced_audio_manager() {
    shutdown();
}

bool nes_enhanced_audio_manager::add_device(std::unique_ptr<audio_device> device) {
    bool result = m_base_manager->add_device(std::move(device));
    if (result) {
        update_device_lists();
    }
    return result;
}

audio_device* nes_enhanced_audio_manager::get_device(const std::string& name) {
    return m_base_manager->get_device(name);
}

std::vector<std::string> nes_enhanced_audio_manager::get_device_names() const {
    return m_base_manager->get_device_names();
}

void nes_enhanced_audio_manager::remove_device(const std::string& name) {
    m_base_manager->remove_device(name);
    update_device_lists();
}

bool nes_enhanced_audio_manager::initialize(uint32_t sample_rate, const nes_audio_mixer::nes_mixer_config& mixer_config) {
    if (m_initialized) {
        return true;
    }

    // Initialize base manager
    if (!m_base_manager->initialize_all(sample_rate)) {
        std::cout << "Failed to initialize base audio device manager" << std::endl;
        return false;
    }

    // Initialize NES mixer
    if (!m_nes_mixer->initialize(sample_rate)) {
        std::cout << "Failed to initialize NES audio mixer" << std::endl;
        return false;
    }

    m_nes_mixer->set_config(mixer_config);
    update_device_lists();

    m_initialized = true;
    std::cout << "Enhanced NES audio manager initialized with " << m_nes_devices.size()
              << " NES devices and " << m_other_devices.size() << " other devices" << std::endl;
    return true;
}

void nes_enhanced_audio_manager::reset() {
    if (m_base_manager) {
        m_base_manager->reset_all();
    }
    if (m_nes_mixer) {
        m_nes_mixer->reset();
    }
}

void nes_enhanced_audio_manager::shutdown() {
    if (m_initialized) {
        if (m_base_manager) {
            m_base_manager->shutdown_all();
        }
        if (m_nes_mixer) {
            m_nes_mixer->shutdown();
        }
        m_initialized = false;
        std::cout << "Enhanced NES audio manager shut down" << std::endl;
    }
}

void nes_enhanced_audio_manager::generate_enhanced_samples(int16_t* buffer, size_t sample_count) {
    if (!m_initialized) {
        std::memset(buffer, 0, sample_count * sizeof(int16_t));
        return;
    }

    if (m_nes_focused_mode && !m_nes_devices.empty()) {
        // Use NES-optimized mixing
        mix_nes_devices_optimized(buffer, sample_count);

        // Mix in other devices if any
        if (!m_other_devices.empty()) {
            if (m_temp_buffer.size() < sample_count) {
                m_temp_buffer.resize(sample_count);
            }
            mix_other_devices_standard(m_temp_buffer.data(), sample_count);

            // Add other devices to NES output
            for (size_t i = 0; i < sample_count; ++i) {
                int32_t mixed = static_cast<int32_t>(buffer[i]) + static_cast<int32_t>(m_temp_buffer[i]);
                if (mixed > 32767) mixed = 32767;
                else if (mixed < -32768) mixed = -32768;
                buffer[i] = static_cast<int16_t>(mixed);
            }
        }
    } else {
        // Fall back to standard mixing
        m_base_manager->generate_mixed_samples(buffer, sample_count);
    }
}

void nes_enhanced_audio_manager::set_mixing_mode(bool nes_focused_mode) {
    m_nes_focused_mode = nes_focused_mode;
    std::cout << "NES-focused mixing " << (nes_focused_mode ? "enabled" : "disabled") << std::endl;
}

nes_enhanced_audio_manager::enhanced_stats nes_enhanced_audio_manager::get_enhanced_stats() const {
    enhanced_stats stats;
    stats.nes_mixer_stats = m_nes_mixer->get_stats();
    stats.active_nes_devices = static_cast<uint32_t>(m_nes_devices.size());
    stats.active_other_devices = static_cast<uint32_t>(m_other_devices.size());
    stats.total_samples_generated = stats.nes_mixer_stats.samples_mixed;
    return stats;
}

void nes_enhanced_audio_manager::update_device_lists() {
    m_nes_devices.clear();
    m_other_devices.clear();

    auto device_names = m_base_manager->get_device_names();
    for (const auto& name : device_names) {
        auto* device = m_base_manager->get_device(name);
        if (device) {
            // Try to cast to NES APU device
            if (auto* nes_device = dynamic_cast<nes_apu_device*>(device)) {
                m_nes_devices.push_back(nes_device);
            } else {
                m_other_devices.push_back(device);
            }
        }
    }
}

void nes_enhanced_audio_manager::mix_nes_devices_optimized(int16_t* buffer, size_t sample_count) {
    if (m_nes_devices.empty()) {
        std::memset(buffer, 0, sample_count * sizeof(int16_t));
        return;
    }

    // For now, use the NES mixer's device mixing function
    m_nes_mixer->mix_nes_devices(m_nes_devices, buffer, sample_count);
}

void nes_enhanced_audio_manager::mix_other_devices_standard(int16_t* buffer, size_t sample_count) {
    if (m_other_devices.empty()) {
        std::memset(buffer, 0, sample_count * sizeof(int16_t));
        return;
    }

    // Use the base manager's standard mixing for non-NES devices
    std::memset(buffer, 0, sample_count * sizeof(int16_t));

    std::vector<int16_t> device_buffer(sample_count);
    std::vector<int32_t> mix_buffer(sample_count, 0);

    for (auto* device : m_other_devices) {
        device->generate_samples(device_buffer.data(), sample_count);
        for (size_t i = 0; i < sample_count; ++i) {
            mix_buffer[i] += device_buffer[i];
        }
    }

    // Copy with clipping
    for (size_t i = 0; i < sample_count; ++i) {
        int32_t sample = mix_buffer[i];
        if (sample > 32767) sample = 32767;
        else if (sample < -32768) sample = -32768;
        buffer[i] = static_cast<int16_t>(sample);
    }
}