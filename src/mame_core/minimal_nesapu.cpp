#include "minimal_nesapu.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

minimal_nesapu_device::minimal_nesapu_device(minimal_machine_config* config, const char* tag, u32 clock)
    : minimal_device_t(config, tag, clock),
      minimal_device_sound_interface(this) {

    std::cout << "Minimal NES APU device '" << tag << "' created @ " << clock << "Hz" << std::endl;
}

minimal_nesapu_device::~minimal_nesapu_device() {
    device_stop();
}

void minimal_nesapu_device::device_start() {
    if (m_started) {
        return;
    }

    std::cout << "Starting minimal NES APU device '" << tag() << "'" << std::endl;

    // Initialize APU state
    device_reset();

    // Allocate sound stream
    allocate_stream(0, 1); // 0 inputs, 1 output
    m_stream_enabled = true;

    set_started(true);
    std::cout << "Minimal NES APU device started successfully" << std::endl;
}

void minimal_nesapu_device::device_reset() {
    std::cout << "Resetting minimal NES APU device" << std::endl;

    // Reset all channels
    m_pulse[0] = {};
    m_pulse[1] = {};
    m_triangle = {};
    m_noise = {};

    // Initialize noise channel shift register
    m_noise.shift_register = 1;

    // Reset status
    m_status_register = 0;
    m_frame_counter = 0;

    std::cout << "Minimal NES APU device reset complete" << std::endl;
}

void minimal_nesapu_device::device_stop() {
    if (!m_started) {
        return;
    }

    std::cout << "Stopping minimal NES APU device '" << tag() << "'" << std::endl;
    m_stream_enabled = false;
    set_started(false);
}

void minimal_nesapu_device::write(offs_t offset, u8 data) {
    if (!m_started) {
        std::cout << "Warning: Write to unstarted NES APU device" << std::endl;
        return;
    }


    switch (offset) {
        case NES_APU_PULSE1_0:
        case NES_APU_PULSE1_1:
        case NES_APU_PULSE1_2:
        case NES_APU_PULSE1_3:
            update_pulse_channel(0, offset, data);
            break;

        case NES_APU_PULSE2_0:
        case NES_APU_PULSE2_1:
        case NES_APU_PULSE2_2:
        case NES_APU_PULSE2_3:
            update_pulse_channel(1, offset - 4, data);
            break;

        case NES_APU_TRIANGLE_0:
        case NES_APU_TRIANGLE_2:
        case NES_APU_TRIANGLE_3:
            update_triangle_channel(offset, data);
            break;

        case NES_APU_NOISE_0:
        case NES_APU_NOISE_2:
        case NES_APU_NOISE_3:
            update_noise_channel(offset, data);
            break;

        case NES_APU_STATUS:
            update_status_register(data);
            break;

        case NES_APU_FRAME:
            m_frame_counter = data;
            break;

        default:
            std::cout << "Unknown NES APU register: $" << std::hex << offset << std::endl;
            break;
    }
}

u8 minimal_nesapu_device::status_r() {
    // Return channel status based on length counters (accurate to NES hardware)
    u8 status = 0;

    if (m_pulse[0].length_counter > 0) status |= 0x01;
    if (m_pulse[1].length_counter > 0) status |= 0x02;
    if (m_triangle.length_counter > 0) status |= 0x04;
    if (m_noise.length_counter > 0) status |= 0x08;

    return status;
}

void minimal_nesapu_device::update_pulse_channel(int channel, offs_t offset, u8 data) {
    pulse_channel& pulse = m_pulse[channel];

    switch (offset & 3) {
        case 0: // Control register
            pulse.duty_cycle = (data >> 6) & 3;
            pulse.envelope_flag = (data & 0x10) != 0;
            pulse.volume = data & 0x0F;
            break;

        case 1: // Sweep register
            // Sweep not implemented in minimal version
            break;

        case 2: // Timer low
            pulse.timer = (pulse.timer & 0xFF00) | data;
            if (pulse.timer > 0) {
                pulse.frequency = clock() / (16 * (pulse.timer + 1));
            }
            break;

        case 3: // Length counter and timer high
            pulse.timer = (pulse.timer & 0x00FF) | ((data & 0x07) << 8);
            pulse.length_counter = data >> 3;
            if (pulse.timer > 0) {
                pulse.frequency = clock() / (16 * (pulse.timer + 1));
            }
            break;
    }
}

void minimal_nesapu_device::update_triangle_channel(offs_t offset, u8 data) {
    switch (offset) {
        case NES_APU_TRIANGLE_0: // Linear counter
            m_triangle.linear_counter = data & 0x7F;
            break;

        case NES_APU_TRIANGLE_2: // Timer low
            m_triangle.timer = (m_triangle.timer & 0xFF00) | data;
            if (m_triangle.timer > 0) {
                m_triangle.frequency = clock() / (32 * (m_triangle.timer + 1));
            }
            break;

        case NES_APU_TRIANGLE_3: // Length counter and timer high
            m_triangle.timer = (m_triangle.timer & 0x00FF) | ((data & 0x07) << 8);
            m_triangle.length_counter = data >> 3;
            if (m_triangle.timer > 0) {
                m_triangle.frequency = clock() / (32 * (m_triangle.timer + 1));
            }
            break;
    }
}

void minimal_nesapu_device::update_noise_channel(offs_t offset, u8 data) {
    switch (offset) {
        case NES_APU_NOISE_0: // Envelope/volume
            m_noise.envelope_flag = (data & 0x10) != 0;
            m_noise.volume = data & 0x0F;
            break;

        case NES_APU_NOISE_2: // Period
            m_noise.period = data & 0x0F;
            m_noise.mode_flag = (data & 0x80) != 0;
            break;

        case NES_APU_NOISE_3: // Length counter
            m_noise.length_counter = data >> 3;
            break;
    }
}

void minimal_nesapu_device::update_status_register(u8 data) {
    m_status_register = data;

    // Enable/disable channels
    m_pulse[0].enabled = (data & 0x01) != 0;
    m_pulse[1].enabled = (data & 0x02) != 0;
    m_triangle.enabled = (data & 0x04) != 0;
    m_noise.enabled = (data & 0x08) != 0;

    // Set length counters to non-zero for enabled channels (for testing)
    if (m_pulse[0].enabled) m_pulse[0].length_counter = 1;
    if (m_pulse[1].enabled) m_pulse[1].length_counter = 1;
    if (m_triangle.enabled) m_triangle.length_counter = 1;
    if (m_noise.enabled) m_noise.length_counter = 1;

    // Clear length counters for disabled channels
    if (!m_pulse[0].enabled) m_pulse[0].length_counter = 0;
    if (!m_pulse[1].enabled) m_pulse[1].length_counter = 0;
    if (!m_triangle.enabled) m_triangle.length_counter = 0;
    if (!m_noise.enabled) m_noise.length_counter = 0;
}

void minimal_nesapu_device::sound_stream_update(s16* buffer, size_t samples) {
    if (!m_stream_enabled || !buffer) {
        return;
    }

    // Clear the output buffer
    memset(buffer, 0, samples * sizeof(s16));


    // Generate audio for each enabled channel
    if (m_pulse[0].enabled) { // Remove length_counter check for now
        static int pulse_calls = 0;
        if (pulse_calls < 5) {
            std::cout << "Generating pulse0: freq=" << m_pulse[0].frequency << " vol=" << (int)m_pulse[0].volume << std::endl;
            pulse_calls++;
        }
        generate_pulse_wave(0, buffer, samples);
    }

    if (m_pulse[1].enabled && m_pulse[1].length_counter > 0) {
        generate_pulse_wave(1, buffer, samples);
    }

    if (m_triangle.enabled && m_triangle.length_counter > 0) {
        generate_triangle_wave(buffer, samples);
    }

    if (m_noise.enabled) { // Remove length_counter check for now
        generate_noise_wave(buffer, samples);
    }

}

void minimal_nesapu_device::generate_pulse_wave(int channel, s16* buffer, size_t samples) {
    pulse_channel& pulse = m_pulse[channel];

    if (pulse.frequency == 0) return;

    // Calculate phase increment for 8-step wave table
    // We want 8 steps per wave period, and frequency cycles per second
    // So: 8 * frequency steps per second, divided by sample rate
    u32 phase_increment = ((u64)pulse.frequency * 8 * 0xFFFFFFFF) / m_sample_rate;

    for (size_t i = 0; i < samples; ++i) {
        u32 wave_index = (pulse.phase_accumulator >> 29) & 7;
        u8 wave_output = PULSE_WAVE_TABLE[pulse.duty_cycle][wave_index];

        s16 sample = 0;
        if (wave_output && pulse.volume > 0) {
            // High volume for audible output
            sample = (PULSE_VOLUME_TABLE[pulse.volume] * 16384) / 15;
        }

        buffer[i] += sample; // Full volume
        pulse.phase_accumulator += phase_increment;
    }
}

void minimal_nesapu_device::generate_triangle_wave(s16* buffer, size_t samples) {
    if (m_triangle.frequency == 0) return;

    u32 phase_increment = (m_triangle.frequency * 0xFFFFFFFF) / m_sample_rate;

    for (size_t i = 0; i < samples; ++i) {
        u32 wave_index = (m_triangle.phase_accumulator >> 27) & 31;
        s16 sample = (TRIANGLE_WAVE[wave_index] * 1536) / 15; // Triangle is louder

        buffer[i] += sample / 4; // Mix with other channels
        m_triangle.phase_accumulator += phase_increment;
    }
}

void minimal_nesapu_device::generate_noise_wave(s16* buffer, size_t samples) {
    u32 period_cycles = NOISE_PERIOD_TABLE[m_noise.period & 0x0F];
    u32 samples_per_cycle = (m_sample_rate * period_cycles) / clock();

    if (samples_per_cycle == 0) samples_per_cycle = 1;

    for (size_t i = 0; i < samples; ++i) {
        if (m_noise.timer == 0) {
            // Update shift register
            u8 feedback = m_noise.shift_register & 1;
            if (m_noise.mode_flag) {
                feedback ^= (m_noise.shift_register >> 6) & 1; // Short mode
            } else {
                feedback ^= (m_noise.shift_register >> 1) & 1; // Long mode
            }

            m_noise.shift_register >>= 1;
            m_noise.shift_register |= (feedback << 14);

            m_noise.timer = samples_per_cycle;
        } else {
            m_noise.timer--;
        }

        s16 sample = 0;
        if ((m_noise.shift_register & 1) && m_noise.volume > 0) {
            sample = (PULSE_VOLUME_TABLE[m_noise.volume] * 16384) / 15; // Higher volume
        }

        buffer[i] += sample; // Full volume for audibility
    }
}

void minimal_nesapu_device::device_clock_changed() {
    std::cout << "NES APU clock changed to " << clock() << "Hz" << std::endl;

    // Recalculate frequencies for all channels
    for (int i = 0; i < 2; ++i) {
        if (m_pulse[i].timer > 0) {
            m_pulse[i].frequency = clock() / (16 * (m_pulse[i].timer + 1));
        }
    }

    if (m_triangle.timer > 0) {
        m_triangle.frequency = clock() / (32 * (m_triangle.timer + 1));
    }
}