#include "mame_midi_player.h"
#include "../mame_integration.h"
#include <iostream>
#include <algorithm>

mame_midi_player::mame_midi_player() {
}

mame_midi_player::~mame_midi_player() {
    // Timer is owned by MAME scheduler, no need to delete
}

void mame_midi_player::initialize(running_machine &machine, mame_nes_apu_device *apu_device) {
    m_machine = &machine;
    m_apu_device = apu_device;

    // Create a timer that fires every millisecond to process MIDI events
    // Using timer_alloc requires a member function pointer via FUNC macro
    // For now, we'll use a simpler approach with timer_set in start_playback
}

void mame_midi_player::load_events(const std::vector<midi_event> &events, uint32_t ticks_per_quarter, uint32_t tempo_us) {
    m_events = events;
    m_ticks_per_quarter = ticks_per_quarter;
    m_tempo_microseconds = tempo_us;
    m_current_event_index = 0;
    m_current_tick = 0;
    m_finished = false;

    // Sort events by tick time
    std::sort(m_events.begin(), m_events.end(),
        [](const midi_event &a, const midi_event &b) { return a.tick < b.tick; });

    // Calculate total duration
    if (!m_events.empty()) {
        uint64_t last_tick = m_events.back().tick;
        double microseconds_per_tick = static_cast<double>(m_tempo_microseconds) / m_ticks_per_quarter;
        double total_microseconds = last_tick * microseconds_per_tick;
        m_duration_ms = static_cast<uint32_t>(total_microseconds / 1000.0) + 1000; // Add 1 second padding
    } else {
        m_duration_ms = 0;
    }

    std::cout << "MAME MIDI Player: Loaded " << m_events.size() << " events, duration: "
              << m_duration_ms << "ms" << std::endl;
}

void mame_midi_player::start_playback() {
    if (!m_machine || !m_apu_device || m_events.empty()) {
        std::cout << "MAME MIDI Player: Cannot start - not initialized or no events" << std::endl;
        return;
    }

    m_playing = true;
    m_current_event_index = 0;
    m_current_tick = 0;

    std::cout << "MAME MIDI Player: Starting playback..." << std::endl;

    // Process all events immediately for now (will refine timing later)
    // This is a simplified version - proper implementation would use MAME timers
    process_events();

    // Schedule machine exit after duration
    attotime duration = attotime::from_msec(m_duration_ms);

    // We can't easily use timers without being a device, so we'll process events immediately
    // and rely on the audio stream to complete before exiting
}

void mame_midi_player::process_events() {
    std::cout << "MAME MIDI Player: Processing all events..." << std::endl;

    for (const auto &event : m_events) {
        switch (event.event_type) {
            case midi_event::type::NOTE_ON:
                std::cout << "  NOTE_ON: ch=" << (int)event.channel
                         << " note=" << (int)event.data1
                         << " vel=" << (int)event.data2 << std::endl;

                if (m_apu_device) {
                    // Write to NES APU registers
                    // For now, map to channel based on MIDI channel
                    uint8_t nes_channel = event.channel % 4; // Skip DMC

                    if (nes_channel < 2) {
                        // Pulse channel
                        uint8_t control = 0x30 | event.data2; // Constant volume
                        m_apu_device->write_register(nes_channel * 4, control);

                        // Calculate timer value for note
                        uint16_t timer = 1789773 / (32 * (440 * pow(2, (event.data1 - 69) / 12.0)));
                        m_apu_device->write_register(nes_channel * 4 + 2, timer & 0xFF);
                        m_apu_device->write_register(nes_channel * 4 + 3, (timer >> 8) | 0xF8);
                    } else if (nes_channel == 2) {
                        // Triangle
                        m_apu_device->write_register(0x08, 0xFF);  // Linear counter
                        uint16_t timer = 1789773 / (32 * (440 * pow(2, (event.data1 - 69) / 12.0)));
                        m_apu_device->write_register(0x0A, timer & 0xFF);
                        m_apu_device->write_register(0x0B, (timer >> 8) | 0xF8);
                    }

                    // Enable channel
                    uint8_t status = (1 << nes_channel);
                    m_apu_device->write_register(0x15, status);
                }
                break;

            case midi_event::type::NOTE_OFF:
                std::cout << "  NOTE_OFF: ch=" << (int)event.channel
                         << " note=" << (int)event.data1 << std::endl;
                // Could disable channel here
                break;

            default:
                break;
        }
    }

    m_finished = true;
    std::cout << "MAME MIDI Player: All events processed" << std::endl;
}

attotime mame_midi_player::ticks_to_attotime(uint64_t ticks) const {
    double microseconds_per_tick = static_cast<double>(m_tempo_microseconds) / m_ticks_per_quarter;
    double microseconds = ticks * microseconds_per_tick;
    return attotime::from_usec(static_cast<uint64_t>(microseconds));
}
