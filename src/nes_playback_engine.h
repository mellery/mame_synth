#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include "music_parser.h"
#include "nes_sequencer.h"
#include "nes_audio_mixer.h"
#include "audio_stream.h"
#include "comprehensive_file_support.h"

// Forward declarations
class nes_enhanced_audio_manager;
class audio_device_manager;
class nes_apu_device;

/**
 * Complete NES Playback Engine - Integrated system for NES music playback
 *
 * This is the main high-level interface that combines all NES synthesizer components:
 * - NES sequencer engine for timing and event processing
 * - NES-focused audio mixer for hardware-accurate sound
 * - Real-time audio streaming with ALSA/DirectSound support
 * - Complete file format support (MIDI, MusicXML, patterns)
 * - Performance monitoring and visualization
 * - Plugin architecture for effects and processing
 */
class nes_playback_engine {
public:
    // Playback engine configuration
    struct engine_config {
        // Audio configuration
        uint32_t sample_rate;
        uint32_t buffer_size;           // Audio buffer size in frames
        audio_stream_factory::backend_type audio_backend;

        // Sequencer configuration
        uint32_t ticks_per_quarter_note;
        uint32_t default_tempo_bpm;     // Default tempo in BPM
        bool enable_looping;
        uint32_t lookahead_ms;          // Event scheduling lookahead

        // NES mixer configuration
        nes_audio_mixer::nes_mixer_config mixer_config;

        // Performance settings
        bool enable_performance_monitoring;
        bool enable_visualization;
        uint32_t max_polyphony;         // Maximum simultaneous notes

        // File format support
        bool enable_midi_support;
        bool enable_musicxml_support;
        bool enable_pattern_support;

        engine_config()
            : sample_rate(44100)
            , buffer_size(1024)
            , audio_backend(audio_stream_factory::backend_type::AUTO)
            , ticks_per_quarter_note(480)
            , default_tempo_bpm(120)
            , enable_looping(false)
            , lookahead_ms(50)
            , enable_performance_monitoring(true)
            , enable_visualization(false)
            , max_polyphony(16)
            , enable_midi_support(true)
            , enable_musicxml_support(true)
            , enable_pattern_support(true) {}
    };

    // Playback state and status
    enum class engine_state {
        UNINITIALIZED,
        INITIALIZED,
        LOADING,
        READY,
        PLAYING,
        PAUSED,
        STOPPING,
        ERROR
    };

    // Comprehensive performance metrics
    struct performance_metrics {
        // Audio performance
        audio_stream::stats audio_stats;
        nes_audio_mixer::mixer_stats mixer_stats;
        nes_sequencer::sequencer_stats sequencer_stats;

        // Engine-specific metrics
        uint64_t total_playback_time_ms = 0;
        uint64_t files_loaded = 0;
        uint64_t playback_sessions = 0;
        double cpu_usage_percentage = 0.0;
        uint32_t memory_usage_kb = 0;

        // Real-time metrics
        float peak_output_level = 0.0f;
        uint32_t active_voices = 0;
        uint32_t dropped_events = 0;
        double average_frame_time_ms = 0.0;

        // Error tracking
        uint32_t audio_underruns = 0;
        uint32_t audio_overruns = 0;
        uint32_t sequencer_errors = 0;
        uint32_t file_load_errors = 0;
    };

    // Event callback system for real-time feedback
    using note_callback_t = std::function<void(uint8_t channel, uint8_t note, uint8_t velocity, bool note_on)>;
    using tempo_callback_t = std::function<void(double bpm, double tempo_scale)>;
    using position_callback_t = std::function<void(music_time_t current_tick, music_time_t total_ticks)>;
    using error_callback_t = std::function<void(const std::string& error_message, int error_code)>;

    explicit nes_playback_engine(const engine_config& config = engine_config{});
    ~nes_playback_engine();

    // Engine lifecycle
    bool initialize();
    bool shutdown();
    bool reset();

    // Configuration management
    void set_config(const engine_config& config);
    engine_config get_config() const { return m_config; }

    // File loading and management
    bool load_file(const std::string& filename);
    bool load_music_data(const music_data& music);
    bool load_pattern_sequence(const nes_pattern_sequencer& pattern_seq);
    void unload_current_music();

    // Enhanced file support with comprehensive metadata
    bool load_file_enhanced(const std::string& filename);
    file_validation_result validate_file(const std::string& filename);
    enhanced_music_metadata get_current_metadata() const;
    bool load_with_nes_optimization(const std::string& filename);
    std::vector<std::string> get_supported_file_formats() const;

    // Playback control
    bool play();
    bool play_from_time(music_time_t start_time);
    bool play_from_percentage(double percentage); // 0.0 to 1.0
    bool pause();
    bool resume();
    bool stop();

    // Transport and positioning
    void seek_to_time(music_time_t time);
    void seek_to_percentage(double percentage);
    music_time_t get_current_position() const;
    music_time_t get_total_duration() const;
    double get_current_percentage() const;

    // Tempo and timing control
    void set_tempo_scale(double scale);
    double get_tempo_scale() const;
    void set_master_tempo_bpm(uint32_t bpm);
    uint32_t get_master_tempo_bpm() const;

    // Loop control
    void set_loop_enabled(bool enabled);
    bool is_loop_enabled() const;
    void set_loop_points(music_time_t start, music_time_t end);
    void set_loop_points_percentage(double start_pct, double end_pct);

    // Audio control
    void set_master_volume(float volume); // 0.0 to 1.0
    float get_master_volume() const;
    void set_channel_volume(uint8_t channel, float volume);
    float get_channel_volume(uint8_t channel) const;
    void mute_channel(uint8_t channel, bool mute);
    bool is_channel_muted(uint8_t channel) const;

    // Real-time interaction
    void trigger_note(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t duration_ms = 1000);
    void stop_note(uint8_t channel, uint8_t note);
    void all_notes_off();
    void panic(); // Emergency stop all sound

    // NES-specific controls
    void set_pulse_duty_cycle(uint8_t channel, uint8_t duty); // 0-3
    void set_triangle_linear_counter(uint8_t value);
    void set_noise_mode(bool short_mode);
    void enable_channel_sweep(uint8_t channel, bool enable, uint8_t rate, bool decrease, uint8_t shift);

    // State and monitoring
    engine_state get_state() const { return m_state; }
    bool is_playing() const { return m_state == engine_state::PLAYING; }
    bool is_ready() const { return m_state == engine_state::READY || m_state == engine_state::PAUSED; }

    performance_metrics get_performance_metrics() const;
    void reset_performance_metrics();

    // Current music information
    struct music_info {
        std::string filename;
        std::string title;
        std::string artist;
        std::string album;
        music_time_t duration_ticks = 0;
        uint32_t duration_seconds = 0;
        uint32_t note_count = 0;
        uint32_t channel_count = 0;
        uint32_t tempo_bpm = 120;
        std::string format; // "MIDI", "MusicXML", "Pattern"
    };
    music_info get_current_music_info() const;

    // Callback registration
    void set_note_callback(note_callback_t callback);
    void set_tempo_callback(tempo_callback_t callback);
    void set_position_callback(position_callback_t callback);
    void set_error_callback(error_callback_t callback);

    // Advanced features
    bool export_to_wav(const std::string& filename, uint32_t sample_rate = 44100);
    bool export_to_midi(const std::string& filename);

    // Pattern creation helpers (for interactive use)
    struct simple_pattern_builder {
        std::vector<music_note> notes;
        std::vector<music_control> controls;
        std::vector<music_program> programs;
        music_time_t duration = 1920; // 4 beats default

        void add_note(uint8_t channel, uint8_t note, uint8_t velocity,
                     music_time_t start, music_time_t duration);
        void add_chord(uint8_t channel, const std::vector<uint8_t>& notes,
                      uint8_t velocity, music_time_t start, music_time_t duration);
        void add_arpeggio(uint8_t channel, const std::vector<uint8_t>& notes,
                         uint8_t velocity, music_time_t start, music_time_t note_duration);
        void add_drum_hit(uint8_t note, uint8_t velocity, music_time_t time);

        music_data to_music_data() const;
    };

    simple_pattern_builder create_pattern_builder();
    bool play_pattern(const simple_pattern_builder& pattern, bool loop = false);

private:
    engine_config m_config;
    std::atomic<engine_state> m_state{engine_state::UNINITIALIZED};
    bool m_initialized = false;

    // Core components
    std::unique_ptr<nes_enhanced_audio_manager> m_audio_manager;
    std::unique_ptr<nes_sequencer> m_sequencer;
    std::unique_ptr<audio_stream> m_audio_stream;
    std::unique_ptr<nes_pattern_sequencer> m_pattern_sequencer;
    std::unique_ptr<comprehensive_file_manager> m_file_manager;

    // Current music state
    music_data m_current_music;
    music_info m_current_info;
    enhanced_music_metadata m_current_metadata;
    std::string m_current_filename;

    // Performance monitoring
    mutable std::mutex m_metrics_mutex;
    performance_metrics m_metrics;
    std::chrono::steady_clock::time_point m_playback_start_time;
    std::chrono::steady_clock::time_point m_last_metrics_update;

    // Callbacks
    note_callback_t m_note_callback;
    tempo_callback_t m_tempo_callback;
    position_callback_t m_position_callback;
    error_callback_t m_error_callback;

    // Audio callback integration
    void audio_callback(int16_t* buffer, size_t frames);

    // Internal methods
    bool initialize_audio_system();
    bool initialize_sequencer_system();
    void shutdown_audio_system();
    void shutdown_sequencer_system();

    void update_performance_metrics();
    void update_current_info();
    void update_current_info_from_metadata();
    void trigger_callbacks();

    // Error handling
    void handle_error(const std::string& message, int error_code = 0);
    void log_debug(const std::string& message);
    void log_info(const std::string& message);
    void log_error(const std::string& message);

    // File format detection and loading
    std::string detect_file_format(const std::string& filename);
    bool load_midi_file(const std::string& filename);
    bool load_musicxml_file(const std::string& filename);

    // Performance optimization
    void optimize_for_realtime();
    void set_thread_priorities();
};

/**
 * NES Playback Engine Factory - Convenience functions for common use cases
 */
class nes_playback_engine_factory {
public:
    // Preset configurations for different use cases
    static nes_playback_engine::engine_config high_quality_config();
    static nes_playback_engine::engine_config low_latency_config();
    static nes_playback_engine::engine_config minimal_cpu_config();
    static nes_playback_engine::engine_config development_config();

    // Quick setup functions
    static std::unique_ptr<nes_playback_engine> create_for_music_playback();
    static std::unique_ptr<nes_playback_engine> create_for_interactive_use();
    static std::unique_ptr<nes_playback_engine> create_for_batch_processing();

    // Validation and diagnostics
    static bool validate_config(const nes_playback_engine::engine_config& config,
                               std::string& error_message);
    static std::vector<std::string> get_supported_formats();
    static bool test_audio_system();
};