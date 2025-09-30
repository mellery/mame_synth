#include "nes_sequencer.h"
#include "nes_audio_mixer.h"
#include "audio_device.h"
#include "audio_stream.h"
#include "debug_config.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <future>

// Sequencer Event Static Constructors
nes_sequencer::sequencer_event nes_sequencer::sequencer_event::note_on(
    sequencer_time_t time, music_time_t tick, uint8_t channel, uint8_t note, uint8_t velocity, music_time_t duration) {
    sequencer_event event;
    event.type = event_type::NOTE_ON;
    event.scheduled_time = time;
    event.tick_time = tick;
    event.data.note_event = {channel, note, velocity, duration};
    return event;
}

nes_sequencer::sequencer_event nes_sequencer::sequencer_event::note_off(
    sequencer_time_t time, music_time_t tick, uint8_t channel, uint8_t note) {
    sequencer_event event;
    event.type = event_type::NOTE_OFF;
    event.scheduled_time = time;
    event.tick_time = tick;
    event.data.note_event = {channel, note, 0, 0};
    return event;
}

nes_sequencer::sequencer_event nes_sequencer::sequencer_event::program_change(
    sequencer_time_t time, music_time_t tick, uint8_t channel, uint8_t program) {
    sequencer_event event;
    event.type = event_type::PROGRAM_CHANGE;
    event.scheduled_time = time;
    event.tick_time = tick;
    event.data.program_event = {channel, program};
    return event;
}

nes_sequencer::sequencer_event nes_sequencer::sequencer_event::control_change(
    sequencer_time_t time, music_time_t tick, uint8_t channel, uint8_t controller, uint8_t value) {
    sequencer_event event;
    event.type = event_type::CONTROL_CHANGE;
    event.scheduled_time = time;
    event.tick_time = tick;
    event.data.control_event = {channel, controller, value};
    return event;
}

nes_sequencer::sequencer_event nes_sequencer::sequencer_event::tempo_change(
    sequencer_time_t time, music_time_t tick, uint32_t microseconds_per_quarter) {
    sequencer_event event;
    event.type = event_type::TEMPO_CHANGE;
    event.scheduled_time = time;
    event.tick_time = tick;
    event.data.tempo_event = {microseconds_per_quarter, 60000000.0 / microseconds_per_quarter};
    return event;
}

// NES Sequencer Implementation
nes_sequencer::nes_sequencer(const sequencer_config& config)
    : m_config(config) {

    // Initialize default NES channel mapping (MIDI channels 0-4 -> NES channels 0-4)
    m_channel_mapping.resize(16);
    for (size_t i = 0; i < m_channel_mapping.size(); ++i) {
        m_channel_mapping[i] = nes_channel_mapping(static_cast<uint8_t>(i),
                                                 static_cast<uint8_t>(std::min(i, size_t(4))),
                                                 i < 5);
    }

    m_microseconds_per_quarter = config.microseconds_per_quarter;
}

nes_sequencer::~nes_sequencer() {
    shutdown();
}

bool nes_sequencer::initialize(nes_enhanced_audio_manager* audio_manager) {
    if (m_initialized || !audio_manager) {
        return false;
    }

    m_audio_manager = audio_manager;
    m_playback_state = playback_state::STOPPED;

    // Reset statistics
    m_stats = {};
    m_last_stats_update = std::chrono::steady_clock::now();

    m_initialized = true;
    std::cout << "NES sequencer initialized with " << m_config.ticks_per_quarter_note
              << " ticks per quarter note" << std::endl;
    return true;
}

bool nes_sequencer::start() {
    if (!m_initialized) {
        return false;
    }

    if (m_config.enable_threading && !m_sequencer_thread) {
        m_thread_running = true;
        m_sequencer_thread = std::make_unique<std::thread>(&nes_sequencer::sequencer_thread_proc, this);
        std::cout << "NES sequencer thread started" << std::endl;
    }

    return true;
}

bool nes_sequencer::stop() {
    if (m_playback_state == playback_state::PLAYING || m_playback_state == playback_state::PAUSED) {
        m_playback_state = playback_state::STOPPING;
        all_notes_off();
        m_playback_state = playback_state::STOPPED;
        m_current_tick = 0;

        std::cout << "NES sequencer stopped" << std::endl;
    }
    return true;
}

bool nes_sequencer::pause() {
    if (m_playback_state == playback_state::PLAYING) {
        m_playback_state = playback_state::PAUSED;
        m_pause_time = std::chrono::steady_clock::now();
        m_pause_tick = m_current_tick.load();
        std::cout << "NES sequencer paused at tick " << m_pause_tick << std::endl;
        return true;
    }
    return false;
}

bool nes_sequencer::resume() {
    if (m_playback_state == playback_state::PAUSED) {
        // Adjust playback start time to account for pause duration
        auto pause_duration = std::chrono::steady_clock::now() - m_pause_time;
        m_playback_start_time += pause_duration;
        m_playback_state = playback_state::PLAYING;
        std::cout << "NES sequencer resumed from tick " << m_pause_tick << std::endl;
        return true;
    }
    return false;
}

void nes_sequencer::shutdown() {
    if (m_initialized) {
        stop();

        // Simple thread cleanup
        if (m_sequencer_thread && m_sequencer_thread->joinable()) {
            std::cout << "Stopping sequencer thread..." << std::endl;
            m_thread_running = false;

            // Signal the thread to wake up
            {
                std::lock_guard<std::mutex> lock(m_thread_mutex);
                m_thread_condition.notify_all();
            }

            m_sequencer_thread->join();
            m_sequencer_thread.reset();
            std::cout << "NES sequencer thread stopped" << std::endl;
        }

        m_audio_manager = nullptr;
        m_initialized = false;

        std::cout << "NES sequencer shut down" << std::endl;
    }
}

bool nes_sequencer::load_music_data(const music_data& music) {
    if (!m_initialized) {
        return false;
    }

    m_music_data = music;

    // Update TPQN from loaded music data (critical for correct timing!)
    if (music.metadata().ticks_per_quarter != m_config.ticks_per_quarter_note) {
        std::cout << "Updating sequencer TPQN: " << m_config.ticks_per_quarter_note
                  << " -> " << music.metadata().ticks_per_quarter << std::endl;
        m_config.ticks_per_quarter_note = music.metadata().ticks_per_quarter;
        std::cout << "DEBUG: After update, m_config.ticks_per_quarter_note = "
                  << m_config.ticks_per_quarter_note << std::endl;
    }

    if (g_debug_config.log_midi_parsing) {
        std::stringstream ss;
        ss << "Loading music data: " << music.notes().size() << " notes, "
           << music.controls().size() << " controls, "
           << music.tempos().size() << " tempo changes"
           << ", TPQN=" << music.metadata().ticks_per_quarter;
        DEBUG_LOG_MIDI(ss.str());
    }

    generate_events_from_music_data();

    std::cout << "Loaded music data: " << m_music_data.notes().size() << " notes, "
              << m_event_queue.size() << " events scheduled" << std::endl;
    return true;
}

bool nes_sequencer::load_music_file(const std::string& filename) {
    if (!m_initialized) {
        return false;
    }

    // Create appropriate parser based on file extension
    std::unique_ptr<music_parser> parser;
    auto ext_pos = filename.find_last_of('.');
    if (ext_pos != std::string::npos) {
        auto ext = filename.substr(ext_pos);
        if (ext == ".mid" || ext == ".midi") {
            parser = std::make_unique<midi_parser>();
        } else if (ext == ".xml" || ext == ".musicxml") {
            parser = std::make_unique<musicxml_parser>();
        }
    }

    if (!parser) {
        std::cout << "Unsupported music file format: " << filename << std::endl;
        return false;
    }

    music_data music;
    if (!parser->parse_file(filename, music)) {
        std::cout << "Failed to parse music file: " << filename << std::endl;
        return false;
    }

    return load_music_data(music);
}

void nes_sequencer::clear_music_data() {
    stop();
    m_music_data = music_data{};

    std::lock_guard<std::mutex> lock(m_event_queue_mutex);
    while (!m_event_queue.empty()) {
        m_event_queue.pop();
    }

    std::cout << "Music data cleared" << std::endl;
}

bool nes_sequencer::play() {
    return play_from_time(0);
}

bool nes_sequencer::play_from_time(music_time_t start_time) {

    if (!m_initialized || m_music_data.notes().empty()) {
        std::cout << "Cannot play: no music data loaded (initialized=" << m_initialized << ", notes=" << m_music_data.notes().size() << ")" << std::endl;
        return false;
    }

    // Stop current playback
    stop();

    // Set playback parameters
    m_current_tick = start_time;
    m_last_scheduled_tick = start_time;  // Reset last scheduled tick when starting playback
    // Set playback start time slightly in the future to ensure proper timing
    m_playback_start_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(10);
    m_playback_state = playback_state::PLAYING;

    // Schedule initial events
    auto lookahead_ticks = (m_config.lookahead_ms * m_config.ticks_per_quarter_note) /
                           (m_microseconds_per_quarter.load() / 1000);
    schedule_events_for_time(start_time, lookahead_ticks);

    if (g_debug_config.log_timing_info) {
        std::stringstream ss;
        ss << "Playback started: tick=" << start_time
           << ", lookahead=" << lookahead_ticks << " ticks"
           << ", total_notes=" << m_music_data.notes().size()
           << ", queue_size=" << m_event_queue.size();
        DEBUG_LOG_TIMING(ss.str());
    }

    // Wake up the sequencer thread to start processing events
    m_thread_condition.notify_one();

    std::cout << "NES sequencer playback started from tick " << start_time << std::endl;
    return true;
}

bool nes_sequencer::play_pattern(const std::vector<music_note>& pattern, bool loop) {
    if (!m_initialized || pattern.empty()) {
        return false;
    }

    // Convert pattern to music data
    music_data pattern_music;
    for (const auto& note : pattern) {
        pattern_music.add_note(note);
    }

    // Set loop parameters if requested
    if (loop) {
        m_loop_enabled = true;
        m_loop_start = 0;
        // Find pattern duration
        music_time_t max_time = 0;
        for (const auto& note : pattern) {
            max_time = std::max(max_time, note.start + note.duration);
        }
        m_loop_end = max_time;
    }

    return load_music_data(pattern_music) && play();
}

void nes_sequencer::set_position(music_time_t time) {
    if (m_playback_state == playback_state::PLAYING || m_playback_state == playback_state::PAUSED) {
        bool was_playing = (m_playback_state == playback_state::PLAYING);
        stop();
        if (was_playing) {
            play_from_time(time);
        } else {
            m_current_tick = time;
        }
    }
}

music_time_t nes_sequencer::get_position() const {
    return m_current_tick.load();
}

music_time_t nes_sequencer::get_total_duration() const {
    music_time_t max_time = 0;
    for (const auto& note : m_music_data.notes()) {
        max_time = std::max(max_time, note.start + note.duration);
    }
    return max_time;
}

void nes_sequencer::set_tempo_scale(double scale) {
    m_tempo_scale = std::clamp(scale, 0.1, 10.0);
    std::cout << "Tempo scale set to " << m_tempo_scale.load() << "x" << std::endl;
}

void nes_sequencer::set_loop_enabled(bool enabled) {
    m_loop_enabled = enabled;
    if (enabled && m_loop_end == 0) {
        m_loop_end = get_total_duration();
    }
    std::cout << "Looping " << (enabled ? "enabled" : "disabled") << std::endl;
}

void nes_sequencer::set_loop_points(music_time_t start, music_time_t end) {
    m_loop_start = start;
    m_loop_end = end;
    std::cout << "Loop points set: " << start << " to " << end << " ticks" << std::endl;
}

void nes_sequencer::set_channel_mapping(const std::vector<nes_channel_mapping>& mapping) {
    if (mapping.size() <= 16) {
        m_channel_mapping = mapping;
        m_channel_mapping.resize(16); // Ensure we have all 16 MIDI channels
        std::cout << "Channel mapping updated" << std::endl;
    }
}

void nes_sequencer::set_channel_enabled(uint8_t midi_channel, bool enabled) {
    if (midi_channel < m_channel_mapping.size()) {
        m_channel_mapping[midi_channel].enabled = enabled;
        std::cout << "MIDI channel " << static_cast<int>(midi_channel)
                  << (enabled ? " enabled" : " disabled") << std::endl;
    }
}

void nes_sequencer::trigger_note(uint8_t channel, uint8_t note, uint8_t velocity, music_time_t duration) {
    if (!m_audio_manager) return;

    uint8_t nes_channel = map_midi_to_nes_channel(channel);
    if (is_channel_enabled(channel) && !is_channel_muted(channel)) {
        music_note music_note_event(nes_channel, note, velocity, 0, duration);

        if (g_debug_config.log_midi_events) {
            std::stringstream ss;
            ss << "NOTE_ON: ch=" << static_cast<int>(channel)
               << " -> NES_ch=" << static_cast<int>(nes_channel)
               << ", note=" << static_cast<int>(note)
               << ", vel=" << static_cast<int>(velocity)
               << ", dur=" << duration << " ticks"
               << ", tick=" << m_current_tick.load();
            DEBUG_LOG_MIDI(ss.str());
        }

        // Try to get NES APU device
        auto* device = m_audio_manager->get_device("NES APU");
        if (device) {
            device->play_note(music_note_event);

            // Schedule note off
            add_active_note(channel, note, duration);
        }
    }
}

void nes_sequencer::stop_note(uint8_t channel, uint8_t note) {
    if (!m_audio_manager) return;

    uint8_t nes_channel = map_midi_to_nes_channel(channel);

    if (g_debug_config.log_midi_events) {
        std::stringstream ss;
        ss << "NOTE_OFF: ch=" << static_cast<int>(channel)
           << " -> NES_ch=" << static_cast<int>(nes_channel)
           << ", note=" << static_cast<int>(note)
           << ", tick=" << m_current_tick.load();
        DEBUG_LOG_MIDI(ss.str());
    }

    auto* device = m_audio_manager->get_device("NES APU");
    if (device) {
        device->stop_note(nes_channel, note);
        remove_active_note(channel, note);
    }
}

void nes_sequencer::all_notes_off() {
    std::lock_guard<std::mutex> lock(m_active_notes_mutex);

    for (const auto& active : m_active_notes) {
        if (m_audio_manager) {
            auto* device = m_audio_manager->get_device("NES APU");
            if (device) {
                device->stop_note(active.channel, active.note);
            }
        }
    }

    m_active_notes.clear();
    std::cout << "All notes stopped" << std::endl;
}

void nes_sequencer::panic() {
    all_notes_off();
    if (m_audio_manager) {
        // Reset all NES devices
        auto device_names = m_audio_manager->get_device_names();
        for (const auto& name : device_names) {
            auto* device = m_audio_manager->get_device(name);
            if (device && name.find("NES") != std::string::npos) {
                device->reset();
            }
        }
    }
    std::cout << "Panic: all sound stopped and devices reset" << std::endl;
}

nes_sequencer::sequencer_stats nes_sequencer::get_stats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

// Private Implementation Methods

void nes_sequencer::sequencer_thread_proc() {

    while (m_thread_running) {
        if (m_playback_state == playback_state::PLAYING && m_thread_running) {
            process_events();

            // Check exit condition again after processing
            if (!m_thread_running) {
                break;
            }

            update_stats();
        }

        // Check exit condition before sleeping
        if (!m_thread_running) {
            break;
        }

        // Sleep for a short time to avoid busy waiting
        std::unique_lock<std::mutex> lock(m_thread_mutex);
        m_thread_condition.wait_for(lock, std::chrono::milliseconds(1), [this] {
            return !m_thread_running || m_playback_state == playback_state::PLAYING;
        });
    }

}

void nes_sequencer::process_events() {
    // Check if we should exit early
    if (!m_thread_running) {
        return;
    }
    auto current_time = std::chrono::steady_clock::now();
    music_time_t current_tick = real_time_to_tick(current_time);

    // Update current position
    m_current_tick = current_tick;

    // Process note-offs for expired notes
    process_note_offs();

    // Check exit condition again
    if (!m_thread_running) {
        return;
    }
    // Process scheduled events
    std::lock_guard<std::mutex> lock(m_event_queue_mutex);
    while (!m_event_queue.empty() && m_event_queue.top().scheduled_time <= current_time && m_thread_running) {
        auto event = m_event_queue.top();
        m_event_queue.pop();

        process_event(event);
        track_event_latency(event);
    }
    // Schedule more events if needed
    auto lookahead_ticks = (m_config.lookahead_ms * m_config.ticks_per_quarter_note) /
                          (m_microseconds_per_quarter.load() / 1000);
    schedule_events_for_time(current_tick, lookahead_ticks);

    // Check for loop condition
    if (m_loop_enabled && current_tick >= m_loop_end) {
        std::cout << "Loop point reached, returning to " << m_loop_start << std::endl;
        set_position(m_loop_start);
    }
}

void nes_sequencer::schedule_events_for_time(music_time_t current_tick, music_time_t lookahead_ticks) {
    auto end_tick = current_tick + lookahead_ticks;

    // Only schedule events after the last scheduled tick to avoid re-scheduling
    auto schedule_start_tick = std::max(current_tick, m_last_scheduled_tick);

    if (schedule_start_tick >= end_tick) {
        // Already scheduled events in this range
        return;
    }

    if (g_debug_config.log_midi_timing) {
        std::stringstream ss;
        ss << "Scheduling events: schedule_start=" << schedule_start_tick
           << ", end_tick=" << end_tick
           << ", lookahead=" << lookahead_ticks << " ticks";
        DEBUG_LOG_TIMING(ss.str());
    }

    int events_scheduled = 0;

    // Schedule note events
    for (const auto& note : m_music_data.notes()) {
        if (note.start >= schedule_start_tick && note.start < end_tick) {
            if (is_channel_enabled(note.channel) && !is_channel_muted(note.channel)) {
                auto note_on_time = tick_to_real_time(note.start);
                auto note_off_time = tick_to_real_time(note.start + note.duration);

                auto note_on_event = sequencer_event::note_on(note_on_time, note.start,
                                                            note.channel, note.note, note.velocity, note.duration);
                auto note_off_event = sequencer_event::note_off(note_off_time, note.start + note.duration,
                                                              note.channel, note.note);

                // Note: mutex already held by caller
                m_event_queue.push(note_on_event);
                m_event_queue.push(note_off_event);
                events_scheduled += 2;
            }
        }
    }

    if (g_debug_config.log_midi_timing && events_scheduled > 0) {
        std::stringstream ss;
        ss << "Scheduled " << events_scheduled << " events in tick range ["
           << current_tick << ", " << end_tick << "]";
        DEBUG_LOG_TIMING(ss.str());
    }

    // Schedule control events
    for (const auto& control : m_music_data.controls()) {
        if (control.time >= schedule_start_tick && control.time < end_tick) {
            auto control_time = tick_to_real_time(control.time);
            auto control_event = sequencer_event::control_change(control_time, control.time,
                                                               control.channel, control.controller, control.value);

            std::lock_guard<std::mutex> lock(m_event_queue_mutex);
            m_event_queue.push(control_event);
        }
    }

    // Schedule program events
    for (const auto& program : m_music_data.programs()) {
        if (program.time >= schedule_start_tick && program.time < end_tick) {
            auto program_time = tick_to_real_time(program.time);
            auto program_event = sequencer_event::program_change(program_time, program.time,
                                                               program.channel, program.program);

            std::lock_guard<std::mutex> lock(m_event_queue_mutex);
            m_event_queue.push(program_event);
        }
    }

    // Schedule tempo events
    for (const auto& tempo : m_music_data.tempos()) {
        if (tempo.time >= schedule_start_tick && tempo.time < end_tick) {
            auto tempo_time = tick_to_real_time(tempo.time);
            auto tempo_event = sequencer_event::tempo_change(tempo_time, tempo.time,
                                                           tempo.microseconds_per_quarter);

            std::lock_guard<std::mutex> lock(m_event_queue_mutex);
            m_event_queue.push(tempo_event);
        }
    }

    // Update last scheduled tick to avoid re-scheduling
    m_last_scheduled_tick = end_tick;
}

void nes_sequencer::process_event(const sequencer_event& event) {
    switch (event.type) {
        case event_type::NOTE_ON:
            trigger_note(event.data.note_event.channel, event.data.note_event.note,
                        event.data.note_event.velocity, event.data.note_event.duration);
            m_stats.notes_played++;
            break;

        case event_type::NOTE_OFF:
            stop_note(event.data.note_event.channel, event.data.note_event.note);
            break;

        case event_type::PROGRAM_CHANGE:
            if (m_audio_manager) {
                auto* device = m_audio_manager->get_device("NES APU");
                if (device) {
                    music_program prog(event.data.program_event.channel, event.data.program_event.program, 0);
                    device->set_program(prog);
                }
            }
            break;

        case event_type::CONTROL_CHANGE:
            if (m_audio_manager) {
                auto* device = m_audio_manager->get_device("NES APU");
                if (device) {
                    music_control ctrl(event.data.control_event.channel,
                                     event.data.control_event.controller,
                                     event.data.control_event.value, 0);
                    device->set_control(ctrl);
                }
            }
            break;

        case event_type::TEMPO_CHANGE:
            m_microseconds_per_quarter = event.data.tempo_event.microseconds_per_quarter;
            std::cout << "Tempo changed to " << event.data.tempo_event.bpm << " BPM" << std::endl;
            break;

        default:
            break;
    }

    m_stats.events_processed++;
}

// Time conversion utilities
nes_sequencer::sequencer_time_t nes_sequencer::tick_to_real_time(music_time_t tick) const {
    auto microseconds_per_tick = (m_microseconds_per_quarter.load() / m_tempo_scale.load()) / m_config.ticks_per_quarter_note;
    auto duration = std::chrono::microseconds(static_cast<int64_t>(tick * microseconds_per_tick));
    return m_playback_start_time + duration;
}

music_time_t nes_sequencer::real_time_to_tick(sequencer_time_t time) const {
    if (m_config.offline_rendering) {
        // Offline mode: use sample count instead of wall-clock time
        uint64_t samples = m_sample_count.load();
        double seconds = static_cast<double>(samples) / m_config.sample_rate;
        double microseconds = seconds * 1000000.0;

        auto microseconds_per_tick = static_cast<double>(m_microseconds_per_quarter.load()) /
                                     (m_tempo_scale.load() * m_config.ticks_per_quarter_note);

        return static_cast<music_time_t>(microseconds / microseconds_per_tick);
    } else {
        // Real-time mode: use wall-clock time
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(time - m_playback_start_time);

        // Guard against negative elapsed time
        if (elapsed.count() < 0) {
            return 0;
        }

        auto microseconds_per_tick = (m_microseconds_per_quarter.load() / m_tempo_scale.load()) / m_config.ticks_per_quarter_note;

        return static_cast<music_time_t>(elapsed.count() / microseconds_per_tick);
    }
}

nes_sequencer::duration_t nes_sequencer::ticks_to_duration(music_time_t ticks) const {
    auto microseconds_per_tick = (m_microseconds_per_quarter.load() / m_tempo_scale.load()) / m_config.ticks_per_quarter_note;
    return std::chrono::microseconds(static_cast<int64_t>(ticks * microseconds_per_tick));
}

void nes_sequencer::set_offline_rendering(bool enabled) {
    m_config.offline_rendering = enabled;
    if (enabled) {
        m_sample_count = 0;  // Reset sample counter
    }
}

void nes_sequencer::advance_samples(uint32_t num_samples) {
    if (m_config.offline_rendering) {
        m_sample_count.fetch_add(num_samples);
    }
}

// Event generation from music data
void nes_sequencer::generate_events_from_music_data() {
    // Clear existing events
    std::lock_guard<std::mutex> lock(m_event_queue_mutex);
    while (!m_event_queue.empty()) {
        m_event_queue.pop();
    }

    // Generate note events from music data
    for (const auto& note : m_music_data.notes()) {
        // Only add if channel is enabled and not muted
        if (is_channel_enabled(note.channel) && !is_channel_muted(note.channel)) {
            // Create note on event using the static constructor
            auto note_on = sequencer_event::note_on(
                tick_to_real_time(note.start), note.start,
                map_midi_to_nes_channel(note.channel), note.note, note.velocity, note.duration
            );
            m_event_queue.push(note_on);

            // Create note off event using the static constructor
            auto note_off = sequencer_event::note_off(
                tick_to_real_time(note.start + note.duration), note.start + note.duration,
                map_midi_to_nes_channel(note.channel), note.note
            );
            m_event_queue.push(note_off);
        }
    }

    // Generate control events
    for (const auto& control : m_music_data.controls()) {
        if (is_channel_enabled(control.channel) && !is_channel_muted(control.channel)) {
            auto ctrl_event = sequencer_event::control_change(
                tick_to_real_time(control.time), control.time,
                map_midi_to_nes_channel(control.channel), control.controller, control.value
            );
            m_event_queue.push(ctrl_event);
        }
    }

    // Generate program change events
    for (const auto& program : m_music_data.programs()) {
        if (is_channel_enabled(program.channel) && !is_channel_muted(program.channel)) {
            auto prog_event = sequencer_event::program_change(
                tick_to_real_time(program.time), program.time,
                map_midi_to_nes_channel(program.channel), program.program
            );
            m_event_queue.push(prog_event);
        }
    }

    std::cout << "Generated events from music data with " << m_config.ticks_per_quarter_note
              << " ticks per quarter note" << std::endl;
}

// Channel mapping helpers
uint8_t nes_sequencer::map_midi_to_nes_channel(uint8_t midi_channel) const {
    if (midi_channel < m_channel_mapping.size()) {
        return m_channel_mapping[midi_channel].nes_channel;
    }
    return std::min(midi_channel, uint8_t(4)); // Default to channel 4 (DMC) for high channels
}

bool nes_sequencer::is_channel_enabled(uint8_t midi_channel) const {
    if (midi_channel < m_channel_mapping.size()) {
        return m_channel_mapping[midi_channel].enabled;
    }
    return false;
}

bool nes_sequencer::is_channel_muted(uint8_t midi_channel) const {
    // For now, no separate mute state - just use enabled/disabled
    return !is_channel_enabled(midi_channel);
}

// Active notes management
void nes_sequencer::add_active_note(uint8_t channel, uint8_t note, music_time_t duration) {
    std::lock_guard<std::mutex> lock(m_active_notes_mutex);

    auto note_off_time = std::chrono::steady_clock::now() + ticks_to_duration(duration);
    m_active_notes.push_back({channel, note, note_off_time});
    m_stats.active_notes++;
}

void nes_sequencer::remove_active_note(uint8_t channel, uint8_t note) {
    std::lock_guard<std::mutex> lock(m_active_notes_mutex);

    auto it = std::find_if(m_active_notes.begin(), m_active_notes.end(),
                          [channel, note](const active_note& active) {
                              return active.channel == channel && active.note == note;
                          });

    if (it != m_active_notes.end()) {
        m_active_notes.erase(it);
        if (m_stats.active_notes > 0) {
            m_stats.active_notes--;
        }
    }
}

void nes_sequencer::process_note_offs() {
    auto current_time = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_active_notes_mutex);

    auto it = std::remove_if(m_active_notes.begin(), m_active_notes.end(),
                            [this, current_time](const active_note& active) {
                                if (current_time >= active.note_off_time) {
                                    stop_note(active.channel, active.note);
                                    return true;
                                }
                                return false;
                            });

    if (it != m_active_notes.end()) {
        m_active_notes.erase(it, m_active_notes.end());
        m_stats.active_notes = static_cast<uint32_t>(m_active_notes.size());
    }
}

// Performance monitoring
void nes_sequencer::update_stats() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);

    auto current_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - m_last_stats_update);

    if (elapsed.count() >= 100) { // Update stats every 100ms
        m_stats.current_tick = m_current_tick.load();
        m_stats.current_bpm = 60000000.0 / (m_microseconds_per_quarter.load() / m_tempo_scale.load());
        m_last_stats_update = current_time;
    }
}

void nes_sequencer::track_event_latency(const sequencer_event& event) {
    auto current_time = std::chrono::steady_clock::now();
    if (current_time > event.scheduled_time) {
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
            current_time - event.scheduled_time).count();

        // Update average latency (simple moving average)
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        if (m_stats.average_latency_ms == 0.0) {
            m_stats.average_latency_ms = latency / 1000.0;
        } else {
            m_stats.average_latency_ms = (m_stats.average_latency_ms * 0.9) + (latency / 1000.0 * 0.1);
        }

        if (latency > 10000) { // More than 10ms late
            m_stats.timing_errors++;
        }
    }
}

// Pattern Sequencer Implementation (Basic)
nes_pattern_sequencer::nes_pattern_sequencer() {
}

nes_pattern_sequencer::~nes_pattern_sequencer() {
}

bool nes_pattern_sequencer::add_pattern(const pattern& pat) {
    // Check if pattern name already exists
    if (find_pattern(pat.name) != nullptr) {
        std::cout << "Pattern '" << pat.name << "' already exists" << std::endl;
        return false;
    }

    m_patterns.push_back(pat);
    std::cout << "Added pattern '" << pat.name << "' with " << pat.notes.size() << " notes" << std::endl;
    return true;
}

bool nes_pattern_sequencer::remove_pattern(const std::string& name) {
    auto it = std::find_if(m_patterns.begin(), m_patterns.end(),
                          [&name](const pattern& p) { return p.name == name; });

    if (it != m_patterns.end()) {
        m_patterns.erase(it);
        std::cout << "Removed pattern '" << name << "'" << std::endl;
        return true;
    }

    return false;
}

nes_pattern_sequencer::pattern* nes_pattern_sequencer::get_pattern(const std::string& name) {
    auto it = std::find_if(m_patterns.begin(), m_patterns.end(),
                          [&name](const pattern& p) { return p.name == name; });
    return (it != m_patterns.end()) ? &(*it) : nullptr;
}

std::vector<std::string> nes_pattern_sequencer::get_pattern_names() const {
    std::vector<std::string> names;
    names.reserve(m_patterns.size());
    for (const auto& pattern : m_patterns) {
        names.push_back(pattern.name);
    }
    return names;
}

void nes_pattern_sequencer::clear_patterns() {
    m_patterns.clear();
    m_song_structure = song_structure{};
    std::cout << "All patterns cleared" << std::endl;
}

void nes_pattern_sequencer::set_song_structure(const song_structure& structure) {
    m_song_structure = structure;
    std::cout << "Song structure set with " << structure.pattern_sequence.size() << " patterns" << std::endl;
}

music_data nes_pattern_sequencer::generate_music_data() const {
    music_data result;
    // Note: music_data doesn't store ticks_per_quarter_note directly

    music_time_t current_time = m_song_structure.intro_length;

    // Process pattern sequence
    for (size_t i = 0; i < m_song_structure.pattern_sequence.size(); ++i) {
        const std::string& pattern_name = m_song_structure.pattern_sequence[i];
        const pattern* pat = find_pattern(pattern_name);

        if (!pat) {
            std::cout << "Warning: Pattern '" << pattern_name << "' not found" << std::endl;
            continue;
        }

        // Get pattern duration (override or original)
        music_time_t pattern_duration = pat->duration;
        if (i < m_song_structure.pattern_durations.size() && m_song_structure.pattern_durations[i] > 0) {
            pattern_duration = m_song_structure.pattern_durations[i];
        }

        // Add pattern events with time offset
        for (const auto& note : pat->notes) {
            music_note offset_note = note;
            offset_note.start += current_time;
            result.add_note(offset_note);
        }

        for (const auto& control : pat->controls) {
            music_control offset_control = control;
            offset_control.time += current_time;
            result.add_control(offset_control);
        }

        for (const auto& program : pat->programs) {
            music_program offset_program = program;
            offset_program.time += current_time;
            result.add_program(offset_program);
        }

        current_time += pattern_duration;
    }

    std::cout << "Generated music data from " << m_patterns.size() << " patterns: "
              << result.notes().size() << " notes total" << std::endl;

    return result;
}

bool nes_pattern_sequencer::load_into_sequencer(nes_sequencer& sequencer) const {
    auto music = generate_music_data();
    return sequencer.load_music_data(music);
}

const nes_pattern_sequencer::pattern* nes_pattern_sequencer::find_pattern(const std::string& name) const {
    auto it = std::find_if(m_patterns.begin(), m_patterns.end(),
                          [&name](const pattern& p) { return p.name == name; });
    return (it != m_patterns.end()) ? &(*it) : nullptr;
}

music_time_t nes_pattern_sequencer::calculate_total_duration() const {
    music_time_t total = m_song_structure.intro_length;

    for (size_t i = 0; i < m_song_structure.pattern_sequence.size(); ++i) {
        const std::string& pattern_name = m_song_structure.pattern_sequence[i];
        const pattern* pat = find_pattern(pattern_name);

        if (pat) {
            music_time_t duration = pat->duration;
            if (i < m_song_structure.pattern_durations.size() && m_song_structure.pattern_durations[i] > 0) {
                duration = m_song_structure.pattern_durations[i];
            }
            total += duration;
        }
    }

    total += m_song_structure.outro_length;
    return total;
}