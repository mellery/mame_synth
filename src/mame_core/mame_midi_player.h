#pragma once

#include "emu.h"
#include <vector>
#include <cstdint>
#include <memory>

// Forward declarations
class running_machine;
class emu_timer;
class mame_nes_apu_device;

/**
 * MAME-integrated MIDI player that runs within MAME's scheduler
 *
 * This player processes MIDI events synchronized with MAME's emulation loop,
 * allowing proper device initialization and register writes.
 */
class mame_midi_player {
public:
    struct midi_event {
        enum class type { NOTE_ON, NOTE_OFF, CONTROL_CHANGE, PROGRAM_CHANGE };

        type event_type;
        uint64_t tick;              // MIDI tick time
        uint8_t channel;
        uint8_t data1;              // note/controller number
        uint8_t data2;              // velocity/value
        uint32_t duration_ticks;    // for NOTE_ON events
    };

    mame_midi_player();
    ~mame_midi_player();

    // Initialize with running_machine
    void initialize(running_machine &machine, mame_nes_apu_device *apu_device);

    // Load MIDI events
    void load_events(const std::vector<midi_event> &events, uint32_t ticks_per_quarter, uint32_t tempo_us);

    // Start playback - this will be called after machine.run() starts
    void start_playback();

    // Get total duration in milliseconds
    uint32_t get_duration_ms() const { return m_duration_ms; }

private:
    // Timer callback - processes MIDI events
    void timer_callback();

    // Process next events at current time
    void process_events();

    // Convert MIDI ticks to attotime
    attotime ticks_to_attotime(uint64_t ticks) const;

    running_machine *m_machine = nullptr;
    mame_nes_apu_device *m_apu_device = nullptr;
    emu_timer *m_timer = nullptr;

    std::vector<midi_event> m_events;
    size_t m_current_event_index = 0;

    uint32_t m_ticks_per_quarter = 480;
    uint32_t m_tempo_microseconds = 500000;  // 120 BPM default
    uint64_t m_current_tick = 0;
    uint32_t m_duration_ms = 0;

    bool m_playing = false;
    bool m_finished = false;
};
