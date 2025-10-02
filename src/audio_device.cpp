#include "audio_device.h"
#include "music_parser.h"
#include "mame_integration.h"
#include "nes_note_mapping.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>

// MIDI note number to frequency conversion (A4 = 440Hz = MIDI note 69)
static double midi_note_to_frequency(uint8_t note_number) {
    return 440.0 * std::pow(2.0, (note_number - 69) / 12.0);
}

// NES APU Device Implementation
nes_apu_device::nes_apu_device(const std::string& tag, uint32_t clock_rate)
    : m_tag(tag), m_clock_rate(clock_rate) {
    // Initialize MAME integration context
    m_mame_context = std::make_unique<mame_machine_context>();

    // Initialize NES note mapping system
    // Assume NTSC region by default (can be made configurable later)
    m_note_mapper = std::make_unique<nes_note_mapping::nes_note_mapper>(
        nes_note_mapping::nes_note_mapper::region_t::NTSC
    );
}

nes_apu_device::~nes_apu_device() {
    shutdown();
}

bool nes_apu_device::initialize(uint32_t sample_rate) {
    if (m_initialized) {
        return true;
    }

    m_sample_rate = sample_rate;

    // Initialize MAME machine context
    if (!m_mame_context->initialize()) {
        std::cout << "Failed to initialize MAME machine context for NES APU" << std::endl;
        return false;
    }

    // Create real MAME NES APU device
    m_mame_device = m_mame_context->create_nes_apu(m_tag, m_clock_rate);
    if (!m_mame_device) {
        std::cout << "Failed to create MAME NES APU device" << std::endl;
        return false;
    }

    // Initialize the MAME device
    if (!m_mame_device->initialize()) {
        std::cout << "Failed to initialize MAME NES APU device" << std::endl;
        return false;
    }

    // Initialize all channels to default state (for compatibility)
    for (auto& channel : m_channels) {
        channel.active = false;
        channel.muted = false;
        channel.volume = 0x0F;
        channel.frequency = 0;
        channel.note_number = 0;
        channel.velocity = 0;
    }

    // Set default APU register values
    m_pulse_duty[0] = 2; // 50% duty cycle
    m_pulse_duty[1] = 2;
    m_triangle_linear = 0;
    m_noise_short_mode = false;
    m_master_volume = 0xFF;

    // Don't sync to MAME device yet - sound_stream isn't created until device_start()
    // We'll sync on the first write_register() call or when playback starts
    // sync_to_mame_device();  // SKIP - device not started yet

    m_initialized = true;
    std::cout << "NES APU device '" << m_tag << "' initialized at " << m_sample_rate << "Hz (with MAME backend)" << std::endl;
    return true;
}

void nes_apu_device::reset() {
    if (!m_initialized) return;

    // Reset MAME device
    if (m_mame_device) {
        m_mame_device->reset();
    }

    // Reset all channels
    for (auto& channel : m_channels) {
        channel.active = false;
        channel.frequency = 0;
        channel.note_number = 0;
        channel.velocity = 0;
    }

    // Reset register values
    m_pulse_duty[0] = 2;
    m_pulse_duty[1] = 2;
    m_triangle_linear = 0;
    m_noise_short_mode = false;

    // Sync state to MAME device (only if device has been started)
    // With lazy initialization, device may not be started yet
    if (m_mame_device && m_mame_device->is_device_started()) {
        sync_to_mame_device();
    }

    std::cout << "NES APU device '" << m_tag << "' reset" << std::endl;
}

void nes_apu_device::shutdown() {
    if (!m_initialized) return;

    // Reset first
    reset();

    // Shutdown MAME device
    if (m_mame_device) {
        m_mame_device->shutdown();
        m_mame_device.reset();
    }

    // Shutdown MAME context
    if (m_mame_context) {
        m_mame_context->shutdown();
    }

    m_initialized = false;
    std::cout << "NES APU device '" << m_tag << "' shut down" << std::endl;
}

bool nes_apu_device::play_note(const music_note& note) {
    if (!m_initialized) return false;

    uint8_t channel = note.channel;

    // Map MIDI channels to NES APU channels
    // Channels 0-1: Pulse channels
    // Channel 2: Triangle channel
    // Channel 3: Noise channel
    // Channel 4: DMC (not implemented for notes)

    if (channel >= 5) {
        // For channels beyond NES capabilities, wrap around
        channel = channel % 4; // Skip DMC for note playback
    }

    if (channel == 4) return false; // DMC doesn't handle notes directly

    m_channels[channel].active = true;
    m_channels[channel].note_number = note.note;
    m_channels[channel].velocity = note.velocity;
    m_channels[channel].volume = map_velocity_to_volume(note.velocity);
    m_channels[channel].frequency = note_to_nes_frequency(note.note);

    std::cout << "NES APU: Playing note " << static_cast<int>(note.note)
              << " on channel " << static_cast<int>(channel)
              << " (freq=" << m_channels[channel].frequency << ")" << std::endl;

    // Sync to MAME device
    if (m_mame_device) {
        sync_to_mame_device();
    }

    return true;
}

bool nes_apu_device::stop_note(uint8_t channel, uint8_t note_number) {
    if (!m_initialized) return false;

    if (channel >= 5) channel = channel % 4;
    if (channel == 4) return false;

    if (m_channels[channel].active && m_channels[channel].note_number == note_number) {
        std::cout << "NES APU: Stopped note " << static_cast<int>(note_number)
                  << " on channel " << static_cast<int>(channel) << std::endl;

        // Mark channel as inactive and sync to MAME device
        m_channels[channel].active = false;
        m_channels[channel].note_number = 0;
        m_channels[channel].velocity = 0;

        // Sync the channel state to the MAME device
        sync_to_mame_device();

        return true;
    }

    return false;
}

bool nes_apu_device::set_program(const music_program& program) {
    if (!m_initialized) return false;

    // NES APU doesn't have traditional program changes, but we can map to waveform settings
    uint8_t channel = program.channel;
    if (channel >= 2) return true; // Only pulse channels support duty cycle changes

    // Map program numbers to duty cycles
    uint8_t duty = 2; // Default 50%
    if (program.program < 32) duty = 0;      // 12.5%
    else if (program.program < 64) duty = 1;  // 25%
    else if (program.program < 96) duty = 2;  // 50%
    else duty = 3;                           // 25% negated

    set_pulse_duty_cycle(channel, duty);

    std::cout << "NES APU: Set program " << static_cast<int>(program.program)
              << " on channel " << static_cast<int>(channel)
              << " (duty=" << static_cast<int>(duty) << ")" << std::endl;

    return true;
}

bool nes_apu_device::set_control(const music_control& control) {
    if (!m_initialized) return false;

    uint8_t channel = control.channel;
    if (channel >= 5) channel = channel % 5;

    switch (control.controller) {
        case 7: // Volume
            set_channel_volume(channel, control.value);
            break;
        case 10: // Pan (not applicable to NES, but acknowledge)
            std::cout << "NES APU: Pan control ignored (mono output)" << std::endl;
            break;
        case 121: // Reset all controllers
            reset();
            break;
        case 123: // All notes off
            m_channels[channel].active = false;
            break;
        default:
            std::cout << "NES APU: Unsupported controller " << static_cast<int>(control.controller) << std::endl;
            break;
    }

    return true;
}

void nes_apu_device::generate_samples(int16_t* buffer, size_t sample_count) {
    if (!m_initialized || !m_mame_device) {
        std::memset(buffer, 0, sample_count * sizeof(int16_t));
        return;
    }

    // Use MAME device to generate real NES APU audio
    m_mame_device->update_audio_stream(buffer, sample_count);

    // Apply master volume scaling
    if (m_master_volume != 0xFF) {
        for (size_t i = 0; i < sample_count; ++i) {
            buffer[i] = static_cast<int16_t>((buffer[i] * m_master_volume) / 255);
        }
    }
}

bool nes_apu_device::is_playing() const {
    for (const auto& channel : m_channels) {
        if (channel.active && !channel.muted) {
            return true;
        }
    }
    return false;
}

void nes_apu_device::set_channel_volume(uint8_t channel, uint8_t volume) {
    if (channel >= 5) return;

    m_channels[channel].volume = (volume * 15) / 127; // Convert MIDI volume to NES 4-bit volume
    std::cout << "NES APU: Set channel " << static_cast<int>(channel)
              << " volume to " << static_cast<int>(m_channels[channel].volume) << std::endl;
}

void nes_apu_device::set_master_volume(uint8_t volume) {
    m_master_volume = volume;
    std::cout << "NES APU: Set master volume to " << static_cast<int>(volume) << std::endl;
}

void nes_apu_device::mute_channel(uint8_t channel, bool mute) {
    if (channel >= 5) return;

    m_channels[channel].muted = mute;
    std::cout << "NES APU: Channel " << static_cast<int>(channel)
              << (mute ? " muted" : " unmuted") << std::endl;
}

void nes_apu_device::set_pulse_duty_cycle(uint8_t channel, uint8_t duty) {
    if (channel >= 2) return;

    m_pulse_duty[channel] = duty & 3; // Clamp to 0-3
    std::cout << "NES APU: Set pulse channel " << static_cast<int>(channel)
              << " duty cycle to " << static_cast<int>(m_pulse_duty[channel]) << std::endl;

    // Sync to MAME device
    if (m_mame_device) {
        uint8_t control_value = 0x40 | (m_pulse_duty[channel] << 6) | m_channels[channel].volume;
        m_mame_device->write_register(channel * 4, control_value);
    }
}

void nes_apu_device::set_triangle_linear_counter(uint8_t value) {
    m_triangle_linear = value;
    std::cout << "NES APU: Set triangle linear counter to " << static_cast<int>(value) << std::endl;

    // Sync to MAME device
    if (m_mame_device) {
        m_mame_device->write_register(0x08, m_triangle_linear);
    }
}

void nes_apu_device::set_noise_mode(bool short_mode) {
    m_noise_short_mode = short_mode;
    std::cout << "NES APU: Set noise mode to " << (short_mode ? "short" : "long") << std::endl;

    // Sync to MAME device
    if (m_mame_device) {
        uint8_t noise_control = m_channels[3].volume;
        if (m_noise_short_mode) {
            noise_control |= 0x80;
        }
        m_mame_device->write_register(0x0C, noise_control);
    }
}

uint16_t nes_apu_device::note_to_nes_frequency(uint8_t note_number) const {
    // Use the enhanced NES note mapping system
    if (m_note_mapper) {
        // Default to pulse channel for frequency calculation (both pulse and triangle can use this)
        return m_note_mapper->note_to_timer(note_number, nes_note_mapping::nes_note_mapper::channel_type_t::PULSE);
    }

    // Fallback to original calculation if note mapper not available
    double frequency = midi_note_to_frequency(note_number);
    uint32_t timer = static_cast<uint32_t>((m_clock_rate / (16.0 * frequency)) - 1);
    return static_cast<uint16_t>(std::min(timer, 2047u));
}

uint8_t nes_apu_device::map_velocity_to_volume(uint8_t velocity) const {
    // Convert MIDI velocity (0-127) to NES volume (0-15)
    return (velocity * 15) / 127;
}

void nes_apu_device::sync_to_mame_device() {
    if (!m_mame_device) {
        return;
    }

    // Sync pulse channels
    for (int i = 0; i < 2; ++i) {
        if (m_channels[i].active) {
            // Control register (duty cycle and volume)
            uint8_t control_value = 0x40 | (m_pulse_duty[i] << 6) | m_channels[i].volume;
            m_mame_device->write_register(i * 4, control_value);

            // Timer low byte - use pulse-specific mapping
            uint16_t timer = m_note_mapper ?
                m_note_mapper->note_to_timer(m_channels[i].note_number, nes_note_mapping::nes_note_mapper::channel_type_t::PULSE) :
                note_to_nes_frequency(m_channels[i].note_number);
            m_mame_device->write_register(i * 4 + 2, timer & 0xFF);

            // Timer high byte with length counter
            m_mame_device->write_register(i * 4 + 3, (timer >> 8) | 0xF8);
        }
    }

    // Sync triangle channel
    if (m_channels[2].active) {
        // Linear counter register
        m_mame_device->write_register(0x08, m_triangle_linear);

        // Timer low byte - use triangle-specific mapping
        uint16_t timer = m_note_mapper ?
            m_note_mapper->note_to_timer(m_channels[2].note_number, nes_note_mapping::nes_note_mapper::channel_type_t::TRIANGLE) :
            note_to_nes_frequency(m_channels[2].note_number);
        m_mame_device->write_register(0x0A, timer & 0xFF);

        // Timer high byte with length counter
        m_mame_device->write_register(0x0B, (timer >> 8) | 0xF8);
    }

    // Sync noise channel
    if (m_channels[3].active) {
        // Volume/envelope register
        uint8_t noise_control = m_channels[3].volume;
        if (m_noise_short_mode) {
            noise_control |= 0x80;
        }
        m_mame_device->write_register(0x0C, noise_control);

        // Noise period register - map note to period
        uint8_t noise_period = m_note_mapper ?
            m_note_mapper->note_to_noise_period(m_channels[3].note_number) :
            0x0F; // Default to highest frequency
        m_mame_device->write_register(0x0E, noise_period | (m_noise_short_mode ? 0x80 : 0x00));

        // Length counter register
        m_mame_device->write_register(0x0F, 0xF8);
    }

    // Enable all active channels
    uint8_t status = 0;
    for (int i = 0; i < 4; ++i) {
        if (m_channels[i].active && !m_channels[i].muted) {
            status |= (1 << i);
        }
    }

    // Debug: Show channel enable status
    static int debug_sync_count = 0;
    if (debug_sync_count < 10) {
        std::cout << "SYNC: Setting status=$" << std::hex << (int)status << std::dec
                  << " (P0=" << (status & 1 ? '1' : '0')
                  << " P1=" << (status & 2 ? '1' : '0')
                  << " T=" << (status & 4 ? '1' : '0')
                  << " N=" << (status & 8 ? '1' : '0') << ")" << std::endl;
        debug_sync_count++;
    }

    m_mame_device->write_register(0x15, status);
}

// SNES DSP Device Implementation
snes_dsp_device::snes_dsp_device(const std::string& tag, uint32_t clock_rate)
    : m_tag(tag), m_clock_rate(clock_rate) {
}

snes_dsp_device::~snes_dsp_device() {
    shutdown();
}

bool snes_dsp_device::initialize(uint32_t sample_rate) {
    if (m_initialized) {
        return true;
    }

    m_sample_rate = sample_rate;

    // Initialize all voices to default state
    for (auto& voice : m_voices) {
        voice.active = false;
        voice.muted = false;
        voice.echo_enable = false;
        voice.noise_enable = false;
        voice.pitch_mod_enable = false;
        voice.volume_left = 0x7F;
        voice.volume_right = 0x7F;
        voice.pitch = 0x1000;
        voice.source_number = 0;
        voice.note_number = 0;
        voice.velocity = 0;
    }

    m_master_volume_left = 0x7F;
    m_master_volume_right = 0x7F;
    m_echo_volume_left = 0;
    m_echo_volume_right = 0;
    m_noise_clock = 0;

    m_initialized = true;
    std::cout << "SNES S-DSP device '" << m_tag << "' initialized at " << m_sample_rate << "Hz" << std::endl;
    return true;
}

void snes_dsp_device::reset() {
    if (!m_initialized) return;

    // Reset all voices
    for (auto& voice : m_voices) {
        voice.active = false;
        voice.pitch = 0x1000;
        voice.note_number = 0;
        voice.velocity = 0;
    }

    std::cout << "SNES S-DSP device '" << m_tag << "' reset" << std::endl;
}

void snes_dsp_device::shutdown() {
    if (!m_initialized) return;

    reset();
    m_initialized = false;
    std::cout << "SNES S-DSP device '" << m_tag << "' shut down" << std::endl;
}

bool snes_dsp_device::play_note(const music_note& note) {
    if (!m_initialized) return false;

    uint8_t voice = note.channel;
    if (voice >= 8) {
        voice = voice % 8; // Wrap to available voices
    }

    m_voices[voice].active = true;
    m_voices[voice].note_number = note.note;
    m_voices[voice].velocity = note.velocity;
    m_voices[voice].volume_left = map_velocity_to_volume(note.velocity);
    m_voices[voice].volume_right = map_velocity_to_volume(note.velocity);
    m_voices[voice].pitch = note_to_snes_pitch(note.note);
    m_voices[voice].source_number = 0; // Default sample

    std::cout << "SNES S-DSP: Playing note " << static_cast<int>(note.note)
              << " on voice " << static_cast<int>(voice)
              << " (pitch=" << m_voices[voice].pitch << ")" << std::endl;

    return true;
}

bool snes_dsp_device::stop_note(uint8_t channel, uint8_t note_number) {
    if (!m_initialized) return false;

    uint8_t voice = channel;
    if (voice >= 8) voice = voice % 8;

    if (m_voices[voice].active && m_voices[voice].note_number == note_number) {
        m_voices[voice].active = false;
        m_voices[voice].velocity = 0;
        std::cout << "SNES S-DSP: Stopped note " << static_cast<int>(note_number)
                  << " on voice " << static_cast<int>(voice) << std::endl;
        return true;
    }

    return false;
}

bool snes_dsp_device::set_program(const music_program& program) {
    if (!m_initialized) return false;

    uint8_t voice = program.channel;
    if (voice >= 8) voice = voice % 8;

    // Map program number to sample/instrument
    m_voices[voice].source_number = program.program % 256; // 8-bit sample number

    std::cout << "SNES S-DSP: Set program " << static_cast<int>(program.program)
              << " on voice " << static_cast<int>(voice)
              << " (source=" << static_cast<int>(m_voices[voice].source_number) << ")" << std::endl;

    return true;
}

bool snes_dsp_device::set_control(const music_control& control) {
    if (!m_initialized) return false;

    uint8_t voice = control.channel;
    if (voice >= 8) voice = voice % 8;

    switch (control.controller) {
        case 7: // Volume
            set_channel_volume(voice, control.value);
            break;
        case 10: // Pan
            // SNES supports stereo panning
            if (control.value < 64) {
                // Pan left
                m_voices[voice].volume_left = 0x7F;
                m_voices[voice].volume_right = (control.value * 0x7F) / 64;
            } else {
                // Pan right
                m_voices[voice].volume_left = ((127 - control.value) * 0x7F) / 64;
                m_voices[voice].volume_right = 0x7F;
            }
            break;
        case 91: // Reverb (map to echo)
            set_echo_enable(voice, control.value > 64);
            break;
        case 121: // Reset all controllers
            reset();
            break;
        case 123: // All notes off
            m_voices[voice].active = false;
            break;
        default:
            std::cout << "SNES S-DSP: Unsupported controller " << static_cast<int>(control.controller) << std::endl;
            break;
    }

    return true;
}

void snes_dsp_device::generate_samples(int16_t* buffer, size_t sample_count) {
    if (!m_initialized) {
        std::memset(buffer, 0, sample_count * sizeof(int16_t));
        return;
    }

    // Simple stub implementation - generates silence for now
    // TODO: Implement actual SNES S-DSP sample playback and filtering
    for (size_t i = 0; i < sample_count; ++i) {
        buffer[i] = 0;
    }

    // In a full implementation, this would:
    // - Play back BRR-compressed samples at specified pitch
    // - Apply gaussian interpolation filtering
    // - Handle echo effects and noise generation
    // - Mix all 8 voices with proper volume and panning
}

bool snes_dsp_device::is_playing() const {
    for (const auto& voice : m_voices) {
        if (voice.active && !voice.muted) {
            return true;
        }
    }
    return false;
}

void snes_dsp_device::set_channel_volume(uint8_t channel, uint8_t volume) {
    if (channel >= 8) return;

    uint8_t snes_volume = (volume * 0x7F) / 127; // Convert MIDI volume to SNES 7-bit volume
    m_voices[channel].volume_left = snes_volume;
    m_voices[channel].volume_right = snes_volume;

    std::cout << "SNES S-DSP: Set voice " << static_cast<int>(channel)
              << " volume to " << static_cast<int>(snes_volume) << std::endl;
}

void snes_dsp_device::set_master_volume(uint8_t volume) {
    uint8_t snes_volume = (volume * 0x7F) / 127;
    m_master_volume_left = snes_volume;
    m_master_volume_right = snes_volume;

    std::cout << "SNES S-DSP: Set master volume to " << static_cast<int>(snes_volume) << std::endl;
}

void snes_dsp_device::mute_channel(uint8_t channel, bool mute) {
    if (channel >= 8) return;

    m_voices[channel].muted = mute;
    std::cout << "SNES S-DSP: Voice " << static_cast<int>(channel)
              << (mute ? " muted" : " unmuted") << std::endl;
}

void snes_dsp_device::set_echo_enable(uint8_t channel, bool enable) {
    if (channel >= 8) return;

    m_voices[channel].echo_enable = enable;
    std::cout << "SNES S-DSP: Voice " << static_cast<int>(channel)
              << " echo " << (enable ? "enabled" : "disabled") << std::endl;
}

void snes_dsp_device::set_noise_enable(uint8_t channel, bool enable) {
    if (channel >= 8) return;

    m_voices[channel].noise_enable = enable;
    std::cout << "SNES S-DSP: Voice " << static_cast<int>(channel)
              << " noise " << (enable ? "enabled" : "disabled") << std::endl;
}

void snes_dsp_device::set_pitch_modulation(uint8_t channel, bool enable) {
    if (channel >= 8) return;

    m_voices[channel].pitch_mod_enable = enable;
    std::cout << "SNES S-DSP: Voice " << static_cast<int>(channel)
              << " pitch modulation " << (enable ? "enabled" : "disabled") << std::endl;
}

uint16_t snes_dsp_device::note_to_snes_pitch(uint8_t note_number) const {
    // SNES S-DSP pitch calculation
    // Pitch register format: 14-bit value where 0x1000 = base sample rate
    double frequency = midi_note_to_frequency(note_number);

    // Assume base sample rate of 32kHz for calculation
    double pitch_multiplier = frequency / 261.626; // C4 as reference (261.626 Hz)

    // SNES pitch is relative to sample playback rate
    uint16_t pitch = static_cast<uint16_t>(0x1000 * pitch_multiplier);

    // Clamp to 14-bit range
    return std::min(pitch, static_cast<uint16_t>(0x3FFF));
}

uint8_t snes_dsp_device::map_velocity_to_volume(uint8_t velocity) const {
    // Convert MIDI velocity (0-127) to SNES volume (0-127, but typically 0-127)
    return (velocity * 0x7F) / 127;
}

// Audio Device Manager Implementation
audio_device_manager::~audio_device_manager() {
    shutdown_all();
    clear_devices();
}

bool audio_device_manager::add_device(std::unique_ptr<audio_device> device) {
    if (!device) return false;

    std::string name = device->get_name();

    // Check for duplicate names
    for (const auto& existing : m_devices) {
        if (existing->get_name() == name) {
            std::cout << "Device with name '" << name << "' already exists" << std::endl;
            return false;
        }
    }

    if (m_initialized) {
        device->initialize(m_sample_rate);
    }

    m_devices.push_back(std::move(device));
    std::cout << "Added audio device '" << name << "'" << std::endl;
    return true;
}

audio_device* audio_device_manager::get_device(const std::string& name) {
    for (auto& device : m_devices) {
        if (device->get_name() == name) {
            return device.get();
        }
    }
    return nullptr;
}

std::vector<std::string> audio_device_manager::get_device_names() const {
    std::vector<std::string> names;
    names.reserve(m_devices.size());

    for (const auto& device : m_devices) {
        names.push_back(device->get_name());
    }

    return names;
}

void audio_device_manager::remove_device(const std::string& name) {
    m_devices.erase(
        std::remove_if(m_devices.begin(), m_devices.end(),
            [&name](const std::unique_ptr<audio_device>& device) {
                return device->get_name() == name;
            }
        ),
        m_devices.end()
    );

    std::cout << "Removed audio device '" << name << "'" << std::endl;
}

void audio_device_manager::clear_devices() {
    m_devices.clear();
    std::cout << "Cleared all audio devices" << std::endl;
}

bool audio_device_manager::initialize_all(uint32_t sample_rate) {
    m_sample_rate = sample_rate;
    bool success = true;

    for (auto& device : m_devices) {
        if (!device->initialize(sample_rate)) {
            std::cout << "Failed to initialize device '" << device->get_name() << "'" << std::endl;
            success = false;
        }
    }

    m_initialized = success && !m_devices.empty();

    // Initialize mix buffer
    m_mix_buffer.resize(8192); // Buffer for mixing samples

    std::cout << "Audio device manager " << (m_initialized ? "initialized" : "failed to initialize")
              << " with " << m_devices.size() << " devices at " << sample_rate << "Hz" << std::endl;

    return m_initialized;
}

void audio_device_manager::reset_all() {
    for (auto& device : m_devices) {
        device->reset();
    }
    std::cout << "Reset all audio devices" << std::endl;
}

void audio_device_manager::shutdown_all() {
    for (auto& device : m_devices) {
        device->shutdown();
    }
    m_initialized = false;
    std::cout << "Shut down all audio devices" << std::endl;
}

bool audio_device_manager::play_note_on_device(const std::string& device_name, const music_note& note) {
    audio_device* device = get_device(device_name);
    if (!device) {
        std::cout << "Device '" << device_name << "' not found" << std::endl;
        return false;
    }

    return device->play_note(note);
}

bool audio_device_manager::stop_note_on_device(const std::string& device_name, uint8_t channel, uint8_t note_number) {
    audio_device* device = get_device(device_name);
    if (!device) {
        std::cout << "Device '" << device_name << "' not found" << std::endl;
        return false;
    }

    return device->stop_note(channel, note_number);
}

bool audio_device_manager::set_program_on_device(const std::string& device_name, const music_program& program) {
    audio_device* device = get_device(device_name);
    if (!device) {
        std::cout << "Device '" << device_name << "' not found" << std::endl;
        return false;
    }

    return device->set_program(program);
}

bool audio_device_manager::set_control_on_device(const std::string& device_name, const music_control& control) {
    audio_device* device = get_device(device_name);
    if (!device) {
        std::cout << "Device '" << device_name << "' not found" << std::endl;
        return false;
    }

    return device->set_control(control);
}

void audio_device_manager::generate_mixed_samples(int16_t* buffer, size_t sample_count) {
    static int mix_call_count = 0;
    if (mix_call_count % 1000 == 0) {
        std::cout << "MANAGER: generate_mixed_samples called " << mix_call_count << " times" << std::endl;
    }
    mix_call_count++;

    if (!m_initialized || m_devices.empty()) {
        std::memset(buffer, 0, sample_count * sizeof(int16_t));
        return;
    }

    // Resize mix buffer if needed
    if (m_mix_buffer.size() < sample_count) {
        m_mix_buffer.resize(sample_count);
    }

    // Clear mix buffer
    std::fill(m_mix_buffer.begin(), m_mix_buffer.begin() + sample_count, 0);

    // Generate samples from each device and mix them
    std::vector<int16_t> device_buffer(sample_count);

    for (auto& device : m_devices) {
        device->generate_samples(device_buffer.data(), sample_count);

        // Mix into accumulator with global volume scaling
        for (size_t i = 0; i < sample_count; ++i) {
            m_mix_buffer[i] += (device_buffer[i] * m_global_volume) / 255;
        }
    }

    // Copy mixed samples to output buffer with clipping
    for (size_t i = 0; i < sample_count; ++i) {
        int32_t sample = m_mix_buffer[i];

        // Clip to 16-bit range
        if (sample > 32767) sample = 32767;
        else if (sample < -32768) sample = -32768;

        buffer[i] = static_cast<int16_t>(sample);
    }
}

bool audio_device_manager::any_device_playing() const {
    for (const auto& device : m_devices) {
        if (device->is_playing()) {
            return true;
        }
    }
    return false;
}

void audio_device_manager::set_global_volume(uint8_t volume) {
    m_global_volume = volume;
    std::cout << "Set global volume to " << static_cast<int>(volume) << std::endl;
}