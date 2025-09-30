#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <queue>
#include <functional>
#include "music_parser.h"

// Forward declarations
class nes_apu_device;
class nes_enhanced_audio_manager;
class audio_stream;

/**
 * NES-focused sequencer engine for real-time music playback
 *
 * Features:
 * - Precise timing control for MIDI/music data playback
 * - NES-specific channel mapping and optimization
 * - Real-time event scheduling and processing
 * - Tempo changes, program changes, and control changes
 * - Loop support and pattern playback
 * - Performance monitoring and latency management
 */
class nes_sequencer {
public:
    using sequencer_time_t = std::chrono::steady_clock::time_point;
    using duration_t = std::chrono::steady_clock::duration;

    // Sequencer event types
    enum class event_type {
        NOTE_ON,
        NOTE_OFF,
        PROGRAM_CHANGE,
        CONTROL_CHANGE,
        TEMPO_CHANGE,
        PATTERN_END,
        LOOP_POINT
    };

    // Scheduled sequencer event
    struct sequencer_event {
        event_type type;
        sequencer_time_t scheduled_time;
        music_time_t tick_time;

        // Event data (union for memory efficiency)
        union {
            struct {
                uint8_t channel;
                uint8_t note;
                uint8_t velocity;
                music_time_t duration;
            } note_event;

            struct {
                uint8_t channel;
                uint8_t program;
            } program_event;

            struct {
                uint8_t channel;
                uint8_t controller;
                uint8_t value;
            } control_event;

            struct {
                uint32_t microseconds_per_quarter;
                double bpm;
            } tempo_event;
        } data;

        // Constructors for different event types
        static sequencer_event note_on(sequencer_time_t time, music_time_t tick,
                                     uint8_t channel, uint8_t note, uint8_t velocity, music_time_t duration);
        static sequencer_event note_off(sequencer_time_t time, music_time_t tick,
                                      uint8_t channel, uint8_t note);
        static sequencer_event program_change(sequencer_time_t time, music_time_t tick,
                                            uint8_t channel, uint8_t program);
        static sequencer_event control_change(sequencer_time_t time, music_time_t tick,
                                            uint8_t channel, uint8_t controller, uint8_t value);
        static sequencer_event tempo_change(sequencer_time_t time, music_time_t tick,
                                          uint32_t microseconds_per_quarter);

        // Comparison for priority queue (earliest events first)
        bool operator>(const sequencer_event& other) const {
            return scheduled_time > other.scheduled_time;
        }
    };

    // Sequencer configuration
    struct sequencer_config {
        uint32_t sample_rate;
        uint32_t ticks_per_quarter_note;    // MIDI ticks per quarter note
        uint32_t microseconds_per_quarter;  // Initial tempo (500000 = 120 BPM)
        bool enable_looping;                // Enable automatic looping
        bool enable_threading;              // Use separate thread for sequencing
        uint32_t lookahead_ms;              // Event scheduling lookahead time
        bool offline_rendering;             // Use sample-based time instead of wall-clock time

        sequencer_config()
            : sample_rate(44100)
            , ticks_per_quarter_note(480)
            , microseconds_per_quarter(500000)  // 120 BPM
            , enable_looping(false)
            , enable_threading(true)
            , lookahead_ms(100)
            , offline_rendering(false) {}
    };

    // Playback state
    enum class playback_state {
        STOPPED,
        PLAYING,
        PAUSED,
        STOPPING
    };

    explicit nes_sequencer(const sequencer_config& config = sequencer_config{});
    ~nes_sequencer();

    // Sequencer lifecycle
    bool initialize(nes_enhanced_audio_manager* audio_manager);
    bool start();
    bool stop();
    bool pause();
    bool resume();
    void shutdown();

    // Music loading and management
    bool load_music_data(const music_data& music);
    bool load_music_file(const std::string& filename);
    void clear_music_data();

    // Playback control
    bool play();
    bool play_from_time(music_time_t start_time);
    bool play_pattern(const std::vector<music_note>& pattern, bool loop = false);

    // Transport control
    void set_position(music_time_t time);
    music_time_t get_position() const;
    music_time_t get_total_duration() const;

    // Offline rendering support
    void set_offline_rendering(bool enabled);    // Enable sample-based timing instead of wall-clock
    void advance_samples(uint32_t num_samples);  // Advance sample counter for offline mode

    void set_tempo_scale(double scale); // 1.0 = normal, 2.0 = double speed, 0.5 = half speed
    double get_tempo_scale() const { return m_tempo_scale; }

    void set_loop_enabled(bool enabled);
    bool is_loop_enabled() const { return m_loop_enabled; }
    void set_loop_points(music_time_t start, music_time_t end);

    // Channel mapping for NES optimization
    struct nes_channel_mapping {
        uint8_t midi_channel;
        uint8_t nes_channel;    // 0-4 (Pulse1, Pulse2, Triangle, Noise, DMC)
        bool enabled;

        nes_channel_mapping(uint8_t midi_ch = 0, uint8_t nes_ch = 0, bool en = true)
            : midi_channel(midi_ch), nes_channel(nes_ch), enabled(en) {}
    };

    void set_channel_mapping(const std::vector<nes_channel_mapping>& mapping);
    void set_channel_enabled(uint8_t midi_channel, bool enabled);
    void mute_channel(uint8_t midi_channel, bool mute);

    // Real-time control
    void trigger_note(uint8_t channel, uint8_t note, uint8_t velocity, music_time_t duration = 480);
    void stop_note(uint8_t channel, uint8_t note);
    void all_notes_off();
    void panic(); // Stop all sound immediately

    // Status and monitoring
    playback_state get_playback_state() const { return m_playback_state; }
    bool is_playing() const { return m_playback_state == playback_state::PLAYING; }
    bool is_paused() const { return m_playback_state == playback_state::PAUSED; }

    // Performance statistics
    struct sequencer_stats {
        uint64_t events_processed = 0;
        uint64_t notes_played = 0;
        uint64_t timing_errors = 0;       // Events that were late
        double average_latency_ms = 0.0;  // Average event processing latency
        double cpu_usage = 0.0;           // CPU usage percentage
        music_time_t current_tick = 0;
        double current_bpm = 120.0;
        uint32_t active_notes = 0;
    };
    sequencer_stats get_stats() const;

private:
    sequencer_config m_config;
    bool m_initialized = false;
    std::atomic<playback_state> m_playback_state{playback_state::STOPPED};

    // Audio system integration
    nes_enhanced_audio_manager* m_audio_manager = nullptr;

    // Music data
    music_data m_music_data;
    std::vector<nes_channel_mapping> m_channel_mapping;

    // Timing and tempo
    std::atomic<music_time_t> m_current_tick{0};
    std::atomic<uint32_t> m_microseconds_per_quarter{500000};
    std::atomic<double> m_tempo_scale{1.0};
    sequencer_time_t m_playback_start_time;
    sequencer_time_t m_pause_time;
    music_time_t m_pause_tick = 0;

    // Offline rendering support
    std::atomic<uint64_t> m_sample_count{0};  // Total samples rendered (for offline mode)

    // Looping
    bool m_loop_enabled = false;
    music_time_t m_loop_start = 0;
    music_time_t m_loop_end = 0;

    // Event scheduling
    std::priority_queue<sequencer_event, std::vector<sequencer_event>, std::greater<sequencer_event>> m_event_queue;
    std::mutex m_event_queue_mutex;
    music_time_t m_last_scheduled_tick = 0;  // Track last scheduled tick to avoid re-scheduling

    // Active notes tracking (for note-off events)
    struct active_note {
        uint8_t channel;
        uint8_t note;
        sequencer_time_t note_off_time;
    };
    std::vector<active_note> m_active_notes;
    mutable std::mutex m_active_notes_mutex;

    // Threading
    std::unique_ptr<std::thread> m_sequencer_thread;
    std::atomic<bool> m_thread_running{false};
    std::condition_variable m_thread_condition;
    std::mutex m_thread_mutex;

    // Statistics
    mutable std::mutex m_stats_mutex;
    sequencer_stats m_stats;
    std::chrono::steady_clock::time_point m_last_stats_update;

    // Internal methods
    void sequencer_thread_proc();
    void process_events();
    void schedule_events_for_time(music_time_t current_tick, music_time_t lookahead_ticks);
    void process_event(const sequencer_event& event);

    // Time conversion utilities
    sequencer_time_t tick_to_real_time(music_time_t tick) const;
    music_time_t real_time_to_tick(sequencer_time_t time) const;
    duration_t ticks_to_duration(music_time_t ticks) const;

    // Event generation from music data
    void generate_events_from_music_data();
    void add_note_events(const music_note& note);
    void add_control_event(const music_control& control);
    void add_program_event(const music_program& program);
    void add_tempo_event(const music_tempo& tempo);

    // Channel mapping helpers
    uint8_t map_midi_to_nes_channel(uint8_t midi_channel) const;
    bool is_channel_enabled(uint8_t midi_channel) const;
    bool is_channel_muted(uint8_t midi_channel) const;

    // Active notes management
    void add_active_note(uint8_t channel, uint8_t note, music_time_t duration);
    void remove_active_note(uint8_t channel, uint8_t note);
    void process_note_offs();

    // Performance monitoring
    void update_stats();
    void track_event_latency(const sequencer_event& event);
};

/**
 * NES Pattern Sequencer - specialized for pattern-based composition
 * Useful for chiptune-style composition with repeating patterns
 */
class nes_pattern_sequencer {
public:
    struct pattern {
        std::string name;
        std::vector<music_note> notes;
        std::vector<music_control> controls;
        std::vector<music_program> programs;
        music_time_t duration;
        bool loop_enabled = false;

        pattern(const std::string& n = "Unnamed", music_time_t dur = 1920) // 4 beats at 480 ticks per beat
            : name(n), duration(dur) {}
    };

    struct song_structure {
        std::vector<std::string> pattern_sequence; // Pattern names in order
        std::vector<music_time_t> pattern_durations; // Override duration if non-zero
        bool loop_entire_song = false;
        music_time_t intro_length = 0;    // Ticks before main loop starts
        music_time_t outro_length = 0;    // Ticks after main loop ends
    };

    nes_pattern_sequencer();
    ~nes_pattern_sequencer();

    // Pattern management
    bool add_pattern(const pattern& pat);
    bool remove_pattern(const std::string& name);
    pattern* get_pattern(const std::string& name);
    std::vector<std::string> get_pattern_names() const;
    void clear_patterns();

    // Song structure
    void set_song_structure(const song_structure& structure);
    const song_structure& get_song_structure() const { return m_song_structure; }

    // Generate complete music data from patterns and structure
    music_data generate_music_data() const;

    // Integration with main sequencer
    bool load_into_sequencer(nes_sequencer& sequencer) const;

private:
    std::vector<pattern> m_patterns;
    song_structure m_song_structure;

    // Helper methods
    const pattern* find_pattern(const std::string& name) const;
    music_time_t calculate_total_duration() const;
};