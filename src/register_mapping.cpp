#include "register_mapping.h"
#include "music_parser.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cstring>

// Base register_mapper implementation
double register_mapper::midi_note_to_frequency(uint8_t note_number) {
    return 440.0 * std::pow(2.0, (note_number - 69) / 12.0);
}

uint8_t register_mapper::clamp_to_range(int32_t value, int32_t min, int32_t max) {
    if (value < min) return static_cast<uint8_t>(min);
    if (value > max) return static_cast<uint8_t>(max);
    return static_cast<uint8_t>(value);
}

// NES APU Register Mapper Implementation
nes_apu_mapper::nes_apu_mapper(uint32_t clock_rate)
    : m_clock_rate(clock_rate) {
    reset_registers();
}

bool nes_apu_mapper::map_note_on(const music_note& note, std::vector<uint8_t>& registers) {
    uint8_t channel = note.channel;
    if (channel >= 5) {
        channel = channel % 4; // Wrap to available channels, skip DMC for now
    }
    if (channel == 4) return false; // DMC doesn't handle note events directly

    // Update channel state
    m_channels[channel].active = true;
    m_channels[channel].note_number = note.note;
    m_channels[channel].velocity = note.velocity;
    m_channels[channel].timer_value = note_to_timer_value(note.note);

    uint32_t base_addr = get_channel_base_address(channel);
    uint8_t volume = velocity_to_volume(note.velocity);

    switch (channel) {
        case 0: // Pulse 1
        case 1: // Pulse 2
        {
            // Control register ($4000/$4004)
            uint8_t control = (m_channels[channel].duty_cycle << 6) |  // Duty cycle
                             (0 << 5) |                                // Length counter halt
                             (1 << 4) |                                // Constant volume
                             volume;                                   // Volume
            write_register(base_addr + 0, control);

            // Sweep register ($4001/$4005) - disable sweep for now
            write_register(base_addr + 1, 0x08);

            // Timer low ($4002/$4006)
            write_register(base_addr + 2, m_channels[channel].timer_value & 0xFF);

            // Timer high + length counter load ($4003/$4007)
            uint8_t timer_high = (0x08 << 3) |                        // Length counter load
                                ((m_channels[channel].timer_value >> 8) & 0x07); // Timer high 3 bits
            write_register(base_addr + 3, timer_high);

            std::cout << "NES APU: Pulse " << static_cast<int>(channel)
                      << " note " << static_cast<int>(note.note)
                      << " timer=" << m_channels[channel].timer_value
                      << " volume=" << static_cast<int>(volume) << std::endl;
            break;
        }

        case 2: // Triangle
        {
            // Linear counter control ($4008)
            uint8_t linear_control = (1 << 7) | 0x7F; // Control flag + linear counter load
            write_register(base_addr + 0, linear_control);

            // Timer low ($400A)
            write_register(base_addr + 2, m_channels[channel].timer_value & 0xFF);

            // Timer high + length counter load ($400B)
            uint8_t timer_high = (0x08 << 3) |
                                ((m_channels[channel].timer_value >> 8) & 0x07);
            write_register(base_addr + 3, timer_high);

            std::cout << "NES APU: Triangle note " << static_cast<int>(note.note)
                      << " timer=" << m_channels[channel].timer_value << std::endl;
            break;
        }

        case 3: // Noise
        {
            // Envelope control ($400C)
            uint8_t envelope = (0 << 5) |  // Length counter halt
                              (1 << 4) |   // Constant volume
                              volume;      // Volume
            write_register(base_addr + 0, envelope);

            // Period and waveform ($400E)
            // Map note to noise period (higher notes = higher frequencies = lower periods)
            uint8_t noise_period = clamp_to_range(127 - note.note, 0, 15);
            uint8_t period_reg = (0 << 7) |   // Short mode flag
                                noise_period; // Period
            write_register(base_addr + 2, period_reg);

            // Length counter load ($400F)
            write_register(base_addr + 3, 0x08 << 3);

            std::cout << "NES APU: Noise note " << static_cast<int>(note.note)
                      << " period=" << static_cast<int>(noise_period)
                      << " volume=" << static_cast<int>(volume) << std::endl;
            break;
        }
    }

    // Enable the channel in $4015
    uint8_t channel_enable = read_register(0x15);
    channel_enable |= (1 << channel);
    write_register(0x15, channel_enable);

    // Copy registers to output vector
    registers.resize(REGISTER_COUNT);
    std::copy(m_registers.begin(), m_registers.end(), registers.begin());

    return true;
}

bool nes_apu_mapper::map_note_off(uint8_t channel, uint8_t note_number, std::vector<uint8_t>& registers) {
    if (channel >= 5) channel = channel % 4;
    if (channel == 4) return false;

    if (!m_channels[channel].active || m_channels[channel].note_number != note_number) {
        return false;
    }

    // Disable the channel in $4015
    uint8_t channel_enable = read_register(0x15);
    channel_enable &= ~(1 << channel);
    write_register(0x15, channel_enable);

    // For pulse and triangle channels, we can also set volume to 0
    if (channel <= 2) {
        uint32_t base_addr = get_channel_base_address(channel);
        uint8_t control = read_register(base_addr);
        control &= 0xF0; // Clear volume bits
        write_register(base_addr, control);
    }

    m_channels[channel].active = false;

    std::cout << "NES APU: Note off channel " << static_cast<int>(channel)
              << " note " << static_cast<int>(note_number) << std::endl;

    // Copy registers to output vector
    registers.resize(REGISTER_COUNT);
    std::copy(m_registers.begin(), m_registers.end(), registers.begin());

    return true;
}

bool nes_apu_mapper::map_program_change(const music_program& program, std::vector<uint8_t>& registers) {
    uint8_t channel = program.channel;
    if (channel >= 2) return true; // Only pulse channels support duty cycle

    // Map MIDI program to duty cycle
    uint8_t duty = 2; // Default 50%
    if (program.program < 32) duty = 0;      // 12.5%
    else if (program.program < 64) duty = 1; // 25%
    else if (program.program < 96) duty = 2; // 50%
    else duty = 3;                           // 25% negated

    set_pulse_duty_cycle(channel, duty);

    std::cout << "NES APU: Program change channel " << static_cast<int>(channel)
              << " program " << static_cast<int>(program.program)
              << " duty=" << static_cast<int>(duty) << std::endl;

    // Copy registers to output vector
    registers.resize(REGISTER_COUNT);
    std::copy(m_registers.begin(), m_registers.end(), registers.begin());

    return true;
}

bool nes_apu_mapper::map_control_change(const music_control& control, std::vector<uint8_t>& registers) {
    uint8_t channel = control.channel;
    if (channel >= 5) channel = channel % 5;

    switch (control.controller) {
        case 7: // Volume
        {
            uint8_t volume = velocity_to_volume(control.value);
            uint32_t base_addr = get_channel_base_address(channel);

            if (channel <= 1) { // Pulse channels
                uint8_t control_reg = read_register(base_addr);
                control_reg = (control_reg & 0xF0) | volume;
                write_register(base_addr, control_reg);
            } else if (channel == 3) { // Noise
                uint8_t envelope_reg = read_register(base_addr);
                envelope_reg = (envelope_reg & 0xF0) | volume;
                write_register(base_addr, envelope_reg);
            }
            // Triangle channel doesn't have volume control
            break;
        }

        case 121: // Reset all controllers
            reset_registers();
            break;

        case 123: // All notes off
            if (channel < 5) {
                uint8_t channel_enable = read_register(0x15);
                channel_enable &= ~(1 << channel);
                write_register(0x15, channel_enable);
                m_channels[channel].active = false;
            }
            break;

        default:
            std::cout << "NES APU: Unsupported controller " << static_cast<int>(control.controller) << std::endl;
            break;
    }

    // Copy registers to output vector
    registers.resize(REGISTER_COUNT);
    std::copy(m_registers.begin(), m_registers.end(), registers.begin());

    return true;
}

uint8_t nes_apu_mapper::read_register(uint32_t address) const {
    if (!is_valid_address(address)) return 0;
    return m_registers[address];
}

void nes_apu_mapper::write_register(uint32_t address, uint8_t value) {
    if (!is_valid_address(address)) return;
    m_registers[address] = value;
}

void nes_apu_mapper::reset_registers() {
    std::fill(m_registers.begin(), m_registers.end(), 0);

    // Reset channel states
    for (auto& channel : m_channels) {
        channel.active = false;
        channel.note_number = 0;
        channel.velocity = 0;
        channel.timer_value = 0;
        channel.duty_cycle = 2; // Default 50%
    }

    // Initialize frame counter to 4-step mode
    write_register(0x17, 0x00);

    std::cout << "NES APU: Registers reset" << std::endl;
}

void nes_apu_mapper::set_pulse_duty_cycle(uint8_t channel, uint8_t duty) {
    if (channel >= 2) return;

    duty &= 3; // Clamp to 0-3
    m_channels[channel].duty_cycle = duty;

    // Update control register if channel is active
    if (m_channels[channel].active) {
        uint32_t base_addr = get_channel_base_address(channel);
        uint8_t control = read_register(base_addr);
        control = (control & 0x3F) | (duty << 6);
        write_register(base_addr, control);
    }
}

void nes_apu_mapper::set_pulse_envelope(uint8_t channel, bool constant_volume, uint8_t volume) {
    if (channel >= 2) return;

    uint32_t base_addr = get_channel_base_address(channel);
    uint8_t control = read_register(base_addr);

    if (constant_volume) {
        control |= (1 << 4);
        control = (control & 0xF0) | (volume & 0x0F);
    } else {
        control &= ~(1 << 4);
    }

    write_register(base_addr, control);
}

void nes_apu_mapper::set_triangle_linear_counter(uint8_t counter_load) {
    uint8_t linear_control = (1 << 7) | (counter_load & 0x7F);
    write_register(0x08, linear_control);
}

void nes_apu_mapper::set_noise_period(uint16_t period, bool short_mode) {
    uint8_t period_reg = (short_mode ? (1 << 7) : 0) | (period & 0x0F);
    write_register(0x0E, period_reg);
}

void nes_apu_mapper::set_channel_enable(uint8_t channel, bool enable) {
    if (channel >= 5) return;

    uint8_t channel_enable = read_register(0x15);
    if (enable) {
        channel_enable |= (1 << channel);
    } else {
        channel_enable &= ~(1 << channel);
    }
    write_register(0x15, channel_enable);
}

uint16_t nes_apu_mapper::note_to_timer_value(uint8_t note_number) const {
    double frequency = midi_note_to_frequency(note_number);

    // NES APU timer calculation: timer = (CPU_CLOCK / (16 * frequency)) - 1
    uint32_t timer = static_cast<uint32_t>((m_clock_rate / (16.0 * frequency)) - 1);

    // Clamp to 11-bit range (0-2047)
    return static_cast<uint16_t>(std::min(timer, 2047u));
}

uint8_t nes_apu_mapper::velocity_to_volume(uint8_t velocity) const {
    // Convert MIDI velocity (0-127) to NES volume (0-15)
    return (velocity * 15) / 127;
}

uint32_t nes_apu_mapper::get_channel_base_address(uint8_t channel) const {
    switch (channel) {
        case 0: return 0x00; // Pulse 1: $4000-$4003
        case 1: return 0x04; // Pulse 2: $4004-$4007
        case 2: return 0x08; // Triangle: $4008-$400B
        case 3: return 0x0C; // Noise: $400C-$400F
        case 4: return 0x10; // DMC: $4010-$4013
        default: return 0x00;
    }
}

bool nes_apu_mapper::is_valid_address(uint32_t address) const {
    return address < REGISTER_COUNT;
}

// SNES S-DSP Register Mapper Implementation
snes_dsp_mapper::snes_dsp_mapper(uint32_t clock_rate)
    : m_clock_rate(clock_rate) {
    reset_registers();
}

bool snes_dsp_mapper::map_note_on(const music_note& note, std::vector<uint8_t>& registers) {
    uint8_t voice = note.channel;
    if (voice >= VOICE_COUNT) {
        voice = voice % VOICE_COUNT; // Wrap to available voices
    }

    // Update voice state
    m_voices[voice].active = true;
    m_voices[voice].note_number = note.note;
    m_voices[voice].velocity = note.velocity;
    m_voices[voice].pitch = note_to_pitch_value(note.note);

    uint8_t volume = velocity_to_volume(note.velocity);
    m_voices[voice].left_volume = volume;
    m_voices[voice].right_volume = volume;

    // Set voice registers
    set_voice_volume(voice, volume, volume);
    set_voice_pitch(voice, m_voices[voice].pitch);
    set_voice_source(voice, m_voices[voice].source_number);

    // Set ADSR envelope (attack=15, decay=7, sustain=7, release=0)
    uint16_t adsr = (15 << 12) | (7 << 8) | (7 << 5) | (0 << 1) | 1; // ADSR enable
    set_voice_adsr(voice, adsr);

    // Key on the voice
    set_key_on(1 << voice);

    std::cout << "SNES S-DSP: Voice " << static_cast<int>(voice)
              << " note " << static_cast<int>(note.note)
              << " pitch=" << m_voices[voice].pitch
              << " volume=" << static_cast<int>(volume) << std::endl;

    // Copy registers to output vector
    registers.resize(REGISTER_COUNT);
    std::copy(m_registers.begin(), m_registers.end(), registers.begin());

    return true;
}

bool snes_dsp_mapper::map_note_off(uint8_t channel, uint8_t note_number, std::vector<uint8_t>& registers) {
    uint8_t voice = channel;
    if (voice >= VOICE_COUNT) voice = voice % VOICE_COUNT;

    if (!m_voices[voice].active || m_voices[voice].note_number != note_number) {
        return false;
    }

    // Key off the voice
    set_key_off(1 << voice);
    m_voices[voice].active = false;

    std::cout << "SNES S-DSP: Voice " << static_cast<int>(voice)
              << " note off " << static_cast<int>(note_number) << std::endl;

    // Copy registers to output vector
    registers.resize(REGISTER_COUNT);
    std::copy(m_registers.begin(), m_registers.end(), registers.begin());

    return true;
}

bool snes_dsp_mapper::map_program_change(const music_program& program, std::vector<uint8_t>& registers) {
    uint8_t voice = program.channel;
    if (voice >= VOICE_COUNT) voice = voice % VOICE_COUNT;

    // Map program number to source/sample number
    uint8_t source_number = program.program % 256;
    m_voices[voice].source_number = source_number;

    set_voice_source(voice, source_number);

    std::cout << "SNES S-DSP: Voice " << static_cast<int>(voice)
              << " program " << static_cast<int>(program.program)
              << " source=" << static_cast<int>(source_number) << std::endl;

    // Copy registers to output vector
    registers.resize(REGISTER_COUNT);
    std::copy(m_registers.begin(), m_registers.end(), registers.begin());

    return true;
}

bool snes_dsp_mapper::map_control_change(const music_control& control, std::vector<uint8_t>& registers) {
    uint8_t voice = control.channel;
    if (voice >= VOICE_COUNT) voice = voice % VOICE_COUNT;

    switch (control.controller) {
        case 7: // Volume
        {
            uint8_t volume = velocity_to_volume(control.value);
            m_voices[voice].left_volume = volume;
            m_voices[voice].right_volume = volume;
            set_voice_volume(voice, volume, volume);
            break;
        }

        case 10: // Pan
        {
            uint8_t base_volume = m_voices[voice].left_volume;
            if (control.value < 64) {
                // Pan left
                m_voices[voice].left_volume = base_volume;
                m_voices[voice].right_volume = (control.value * base_volume) / 64;
            } else {
                // Pan right
                m_voices[voice].left_volume = ((127 - control.value) * base_volume) / 64;
                m_voices[voice].right_volume = base_volume;
            }
            set_voice_volume(voice, m_voices[voice].left_volume, m_voices[voice].right_volume);
            break;
        }

        case 91: // Reverb (map to echo)
        {
            uint8_t echo_flags = read_register(ECHO_FLAGS);
            if (control.value > 64) {
                echo_flags |= (1 << voice);
            } else {
                echo_flags &= ~(1 << voice);
            }
            write_register(ECHO_FLAGS, echo_flags);
            break;
        }

        case 121: // Reset all controllers
            reset_registers();
            break;

        case 123: // All notes off
            set_key_off(1 << voice);
            m_voices[voice].active = false;
            break;

        default:
            std::cout << "SNES S-DSP: Unsupported controller " << static_cast<int>(control.controller) << std::endl;
            break;
    }

    // Copy registers to output vector
    registers.resize(REGISTER_COUNT);
    std::copy(m_registers.begin(), m_registers.end(), registers.begin());

    return true;
}

uint8_t snes_dsp_mapper::read_register(uint32_t address) const {
    if (!is_valid_address(address)) return 0;
    return m_registers[address];
}

void snes_dsp_mapper::write_register(uint32_t address, uint8_t value) {
    if (!is_valid_address(address)) return;
    m_registers[address] = value;
}

void snes_dsp_mapper::reset_registers() {
    std::fill(m_registers.begin(), m_registers.end(), 0);

    // Reset voice states
    for (auto& voice : m_voices) {
        voice.active = false;
        voice.note_number = 0;
        voice.velocity = 0;
        voice.pitch = 0x1000; // Default pitch
        voice.source_number = 0;
        voice.left_volume = 0x7F;
        voice.right_volume = 0x7F;
    }

    // Initialize global registers to reasonable defaults
    set_master_volume(0x7F, 0x7F);
    set_echo_volume(0x00, 0x00); // Echo disabled by default

    std::cout << "SNES S-DSP: Registers reset" << std::endl;
}

void snes_dsp_mapper::set_voice_volume(uint8_t voice, uint8_t left_vol, uint8_t right_vol) {
    if (voice >= VOICE_COUNT) return;

    write_register(get_voice_register_address(voice, VOICE_LEFT_VOLUME), left_vol);
    write_register(get_voice_register_address(voice, VOICE_RIGHT_VOLUME), right_vol);
}

void snes_dsp_mapper::set_voice_pitch(uint8_t voice, uint16_t pitch) {
    if (voice >= VOICE_COUNT) return;

    write_register(get_voice_register_address(voice, VOICE_PITCH_LOW), pitch & 0xFF);
    write_register(get_voice_register_address(voice, VOICE_PITCH_HIGH), (pitch >> 8) & 0x3F);
}

void snes_dsp_mapper::set_voice_source(uint8_t voice, uint8_t source_number) {
    if (voice >= VOICE_COUNT) return;

    write_register(get_voice_register_address(voice, VOICE_SOURCE_NUMBER), source_number);
}

void snes_dsp_mapper::set_voice_adsr(uint8_t voice, uint16_t adsr) {
    if (voice >= VOICE_COUNT) return;

    write_register(get_voice_register_address(voice, VOICE_ADSR_LOW), adsr & 0xFF);
    write_register(get_voice_register_address(voice, VOICE_ADSR_HIGH), (adsr >> 8) & 0xFF);
}

void snes_dsp_mapper::set_master_volume(uint8_t left_vol, uint8_t right_vol) {
    write_register(MASTER_LEFT_VOLUME, left_vol);
    write_register(MASTER_RIGHT_VOLUME, right_vol);
}

void snes_dsp_mapper::set_echo_volume(uint8_t left_vol, uint8_t right_vol) {
    write_register(ECHO_LEFT_VOLUME, left_vol);
    write_register(ECHO_RIGHT_VOLUME, right_vol);
}

void snes_dsp_mapper::set_key_on(uint8_t voice_mask) {
    write_register(KEY_ON, voice_mask);
}

void snes_dsp_mapper::set_key_off(uint8_t voice_mask) {
    write_register(KEY_OFF, voice_mask);
}

uint16_t snes_dsp_mapper::note_to_pitch_value(uint8_t note_number) const {
    double frequency = midi_note_to_frequency(note_number);

    // SNES S-DSP pitch calculation
    // Pitch register is a 14-bit value where 0x1000 = base sample rate
    // Higher values = higher pitch
    double pitch_multiplier = frequency / 261.626; // C4 as reference (261.626 Hz)

    uint16_t pitch = static_cast<uint16_t>(0x1000 * pitch_multiplier);

    // Clamp to 14-bit range
    return std::min(pitch, static_cast<uint16_t>(0x3FFF));
}

uint8_t snes_dsp_mapper::velocity_to_volume(uint8_t velocity) const {
    // Convert MIDI velocity (0-127) to SNES volume (0-127)
    return (velocity * 0x7F) / 127;
}

uint32_t snes_dsp_mapper::get_voice_register_address(uint8_t voice, voice_register_offset offset) const {
    if (voice >= VOICE_COUNT) return 0;
    return (voice << 4) | offset;
}

bool snes_dsp_mapper::is_valid_address(uint32_t address) const {
    return address < REGISTER_COUNT;
}

bool snes_dsp_mapper::is_voice_register(uint32_t address) const {
    return (address & 0x0F) <= 0x09; // Voice registers are 0x00-0x09 per voice
}

uint8_t snes_dsp_mapper::get_voice_from_address(uint32_t address) const {
    return (address >> 4) & 0x07;
}

// Register mapping factory implementation
std::unique_ptr<register_mapper> register_mapping_factory::create_mapper(device_type type, uint32_t clock_rate) {
    switch (type) {
        case NES_APU:
            if (clock_rate == 0) clock_rate = nes_apu_mapper::BASE_CLOCK_NTSC;
            return std::make_unique<nes_apu_mapper>(clock_rate);

        case SNES_DSP:
            if (clock_rate == 0) clock_rate = snes_dsp_mapper::BASE_CLOCK;
            return std::make_unique<snes_dsp_mapper>(clock_rate);

        default:
            return nullptr;
    }
}

std::vector<register_mapping_factory::device_type> register_mapping_factory::supported_devices() {
    return {NES_APU, SNES_DSP};
}

std::string register_mapping_factory::device_type_name(device_type type) {
    switch (type) {
        case NES_APU: return "NES APU";
        case SNES_DSP: return "SNES S-DSP";
        default: return "Unknown";
    }
}

// Register diff utility implementation
std::vector<register_diff::change> register_diff::compare(const register_snapshot& before,
                                                         const register_snapshot& after) {
    std::vector<change> changes;

    if (before.device_name != after.device_name) {
        return changes; // Can't compare different devices
    }

    size_t min_size = std::min(before.register_data.size(), after.register_data.size());

    for (size_t i = 0; i < min_size; ++i) {
        if (before.register_data[i] != after.register_data[i]) {
            changes.push_back({
                static_cast<uint32_t>(i),
                before.register_data[i],
                after.register_data[i]
            });
        }
    }

    return changes;
}

std::string register_diff::format_changes(const std::vector<change>& changes) {
    std::ostringstream oss;

    for (const auto& change : changes) {
        oss << "  $" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
            << change.address << ": $" << std::setw(2) << static_cast<int>(change.old_value)
            << " -> $" << std::setw(2) << static_cast<int>(change.new_value) << std::endl;
    }

    return oss.str();
}

void register_diff::print_changes(const std::vector<change>& changes, const std::string& device_name) {
    if (changes.empty()) {
        std::cout << device_name << ": No register changes" << std::endl;
        return;
    }

    std::cout << device_name << " register changes (" << changes.size() << "):" << std::endl;
    std::cout << format_changes(changes);
}