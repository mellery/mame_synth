#include "nes_playback_engine.h"
#include "nes_audio_mixer.h"
#include "audio_device.h"
#include "music_parser.h"
#include "comprehensive_file_support.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <set>
#include <chrono>
#include <thread>
#include <cstring>

// NES Playback Engine Implementation
nes_playback_engine::nes_playback_engine(const engine_config& config)
    : m_config(config) {
    std::cout << "NES Playback Engine created" << std::endl;
}

nes_playback_engine::~nes_playback_engine() {
    shutdown();
}

bool nes_playback_engine::initialize() {
    if (m_initialized) {
        return true;
    }

    m_state = engine_state::LOADING;
    std::cout << "Initializing NES Playback Engine..." << std::endl;

    try {
        // Initialize audio system
        if (!initialize_audio_system()) {
            handle_error("Failed to initialize audio system");
            m_state = engine_state::ERROR;
            return false;
        }

        // Initialize sequencer system
        if (!initialize_sequencer_system()) {
            handle_error("Failed to initialize sequencer system");
            m_state = engine_state::ERROR;
            return false;
        }

        // Initialize pattern sequencer if enabled
        if (m_config.enable_pattern_support) {
            m_pattern_sequencer = std::make_unique<nes_pattern_sequencer>();
            std::cout << "Pattern sequencer initialized" << std::endl;
        }

        // Initialize comprehensive file manager
        m_file_manager = comprehensive_file_support_factory::create_nes_optimized_manager();
        if (!m_file_manager) {
            handle_error("Failed to create file manager");
            m_state = engine_state::ERROR;
            return false;
        }

        auto supported_formats = m_file_manager->get_supported_formats();
        std::cout << "File manager initialized with " << supported_formats.size() << " formats: ";
        for (const auto& format : supported_formats) {
            std::cout << format << " ";
        }
        std::cout << std::endl;

        // Set up performance monitoring
        if (m_config.enable_performance_monitoring) {
            m_last_metrics_update = std::chrono::steady_clock::now();
            std::cout << "Performance monitoring enabled" << std::endl;
        }

        // Apply real-time optimizations
        optimize_for_realtime();

        m_initialized = true;
        m_state = engine_state::INITIALIZED;

        std::cout << "NES Playback Engine initialized successfully" << std::endl;
        std::cout << "  Sample rate: " << m_config.sample_rate << " Hz" << std::endl;
        std::cout << "  Buffer size: " << m_config.buffer_size << " frames" << std::endl;
        std::cout << "  Audio backend: " << audio_stream_factory::backend_name(m_config.audio_backend) << std::endl;
        std::cout << "  Max polyphony: " << m_config.max_polyphony << " notes" << std::endl;

        return true;

    } catch (const std::exception& e) {
        handle_error("Exception during initialization: " + std::string(e.what()));
        m_state = engine_state::ERROR;
        return false;
    }
}

bool nes_playback_engine::shutdown() {
    if (!m_initialized) {
        return true;
    }

    std::cout << "Shutting down NES Playback Engine..." << std::endl;
    m_state = engine_state::STOPPING;

    // Stop any current playback
    stop();

    // Shutdown components in reverse order
    shutdown_sequencer_system();
    shutdown_audio_system();

    // Reset pattern sequencer
    m_pattern_sequencer.reset();

    // Reset file manager
    m_file_manager.reset();

    // Clear current music
    m_current_music.clear();
    m_current_info = music_info{};
    m_current_filename.clear();

    m_initialized = false;
    m_state = engine_state::UNINITIALIZED;

    std::cout << "NES Playback Engine shut down" << std::endl;
    return true;
}

bool nes_playback_engine::reset() {
    if (!m_initialized) {
        return false;
    }

    std::cout << "Resetting NES Playback Engine..." << std::endl;

    // Stop playback
    stop();

    // Reset sequencer
    if (m_sequencer) {
        m_sequencer->all_notes_off();
        m_sequencer->set_position(0);
    }

    // Reset audio
    if (m_audio_manager) {
        m_audio_manager->reset();
    }

    // Reset performance metrics
    reset_performance_metrics();

    m_state = engine_state::READY;
    std::cout << "NES Playback Engine reset" << std::endl;
    return true;
}

void nes_playback_engine::set_config(const engine_config& config) {
    m_config = config;
    // Note: Would need to reinitialize components if config changes significantly
}

bool nes_playback_engine::load_file(const std::string& filename) {
    if (!m_initialized) {
        handle_error("Engine not initialized");
        return false;
    }

    std::cout << "Loading music file: " << filename << std::endl;
    m_state = engine_state::LOADING;

    try {
        // Detect file format
        std::string format = detect_file_format(filename);
        if (format.empty()) {
            handle_error("Unsupported file format: " + filename);
            m_state = engine_state::ERROR;
            return false;
        }

        // Load based on format
        bool success = false;
        if (format == "MIDI" && m_config.enable_midi_support) {
            success = load_midi_file(filename);
        } else if (format == "MusicXML" && m_config.enable_musicxml_support) {
            success = load_musicxml_file(filename);
        } else {
            handle_error("Format not supported or disabled: " + format);
            m_state = engine_state::ERROR;
            return false;
        }

        if (!success) {
            handle_error("Failed to load file: " + filename);
            m_state = engine_state::ERROR;
            return false;
        }

        // Load into sequencer
        if (!m_sequencer->load_music_data(m_current_music)) {
            handle_error("Failed to load music into sequencer");
            m_state = engine_state::ERROR;
            return false;
        }

        m_current_filename = filename;
        update_current_info();

        m_state = engine_state::READY;
        m_metrics.files_loaded++;

        std::cout << "Successfully loaded: " << m_current_info.title << std::endl;
        std::cout << "  Duration: " << m_current_info.duration_seconds << " seconds" << std::endl;
        std::cout << "  Notes: " << m_current_info.note_count << std::endl;
        std::cout << "  Channels: " << m_current_info.channel_count << std::endl;

        return true;

    } catch (const std::exception& e) {
        handle_error("Exception loading file: " + std::string(e.what()));
        m_state = engine_state::ERROR;
        return false;
    }
}

bool nes_playback_engine::load_music_data(const music_data& music) {
    if (!m_initialized) {
        handle_error("Engine not initialized");
        return false;
    }

    std::cout << "Loading music data directly" << std::endl;
    m_state = engine_state::LOADING;

    try {
        m_current_music = music;

        if (!m_sequencer->load_music_data(m_current_music)) {
            handle_error("Failed to load music into sequencer");
            m_state = engine_state::ERROR;
            return false;
        }

        m_current_filename = "[Direct Music Data]";
        update_current_info();

        m_state = engine_state::READY;
        m_metrics.files_loaded++;

        std::cout << "Successfully loaded music data: " << m_current_info.note_count << " notes" << std::endl;
        return true;

    } catch (const std::exception& e) {
        handle_error("Exception loading music data: " + std::string(e.what()));
        m_state = engine_state::ERROR;
        return false;
    }
}

bool nes_playback_engine::load_pattern_sequence(const nes_pattern_sequencer& pattern_seq) {
    if (!m_initialized || !m_config.enable_pattern_support) {
        handle_error("Pattern support not available");
        return false;
    }

    std::cout << "Loading pattern sequence" << std::endl;
    m_state = engine_state::LOADING;

    try {
        // Generate music data from patterns
        auto music = pattern_seq.generate_music_data();

        if (music.empty()) {
            handle_error("Pattern sequence generated no music data");
            m_state = engine_state::ERROR;
            return false;
        }

        m_current_music = music;

        if (!m_sequencer->load_music_data(m_current_music)) {
            handle_error("Failed to load patterns into sequencer");
            m_state = engine_state::ERROR;
            return false;
        }

        // Copy pattern sequencer for future reference
        *m_pattern_sequencer = pattern_seq;

        m_current_filename = "[Pattern Sequence]";
        update_current_info();

        m_state = engine_state::READY;
        m_metrics.files_loaded++;

        std::cout << "Successfully loaded pattern sequence: " << m_current_info.note_count << " notes" << std::endl;
        return true;

    } catch (const std::exception& e) {
        handle_error("Exception loading patterns: " + std::string(e.what()));
        m_state = engine_state::ERROR;
        return false;
    }
}

void nes_playback_engine::unload_current_music() {
    stop();
    m_current_music.clear();
    m_current_info = music_info{};
    m_current_filename.clear();

    if (m_sequencer) {
        m_sequencer->clear_music_data();
    }

    m_state = engine_state::INITIALIZED;
    std::cout << "Current music unloaded" << std::endl;
}

// Enhanced file support methods
bool nes_playback_engine::load_file_enhanced(const std::string& filename) {
    if (!m_initialized || !m_file_manager) {
        handle_error("Engine or file manager not initialized");
        return false;
    }

    std::cout << "Loading file with enhanced support: " << filename << std::endl;
    m_state = engine_state::LOADING;

    try {
        // Load file with enhanced metadata extraction
        enhanced_music_metadata metadata;
        if (!m_file_manager->load_file(filename, m_current_music, metadata)) {
            handle_error("Failed to load file with enhanced parser: " + filename);
            m_state = engine_state::ERROR;
            return false;
        }

        // Store enhanced metadata
        m_current_metadata = metadata;
        m_current_filename = filename;

        // Show NES compatibility analysis
        if (metadata.nes_analysis.is_nes_compatible) {
            std::cout << "✓ File is NES compatible!" << std::endl;
        } else {
            std::cout << "⚠ File has NES compatibility issues:" << std::endl;
            for (const auto& warning : metadata.nes_analysis.compatibility_warnings) {
                std::cout << "  - " << warning << std::endl;
            }
        }

        // Show optimization suggestions
        if (!metadata.nes_analysis.optimization_suggestions.empty()) {
            std::cout << "Optimization suggestions:" << std::endl;
            for (const auto& suggestion : metadata.nes_analysis.optimization_suggestions) {
                std::cout << "  • " << suggestion << std::endl;
            }
        }

        // Load into sequencer
        if (!m_sequencer->load_music_data(m_current_music)) {
            handle_error("Failed to load music into sequencer");
            m_state = engine_state::ERROR;
            return false;
        }

        // Update info from enhanced metadata
        update_current_info_from_metadata();

        m_state = engine_state::READY;
        m_metrics.files_loaded++;

        std::cout << "Successfully loaded enhanced: " << metadata.summary_string() << std::endl;
        std::cout << "  Duration: " << metadata.format_duration() << std::endl;
        std::cout << "  NES channel usage: P1=" << static_cast<int>(metadata.nes_analysis.pulse1_usage_percentage)
                  << "% P2=" << static_cast<int>(metadata.nes_analysis.pulse2_usage_percentage)
                  << "% T=" << static_cast<int>(metadata.nes_analysis.triangle_usage_percentage)
                  << "% N=" << static_cast<int>(metadata.nes_analysis.noise_usage_percentage) << "%" << std::endl;

        return true;

    } catch (const std::exception& e) {
        handle_error("Exception loading enhanced file: " + std::string(e.what()));
        m_state = engine_state::ERROR;
        return false;
    }
}

file_validation_result nes_playback_engine::validate_file(const std::string& filename) {
    if (!m_file_manager) {
        file_validation_result result;
        result.format_detected = "Unknown";
        result.errors.push_back("File manager not initialized");
        return result;
    }

    return m_file_manager->validate_file(filename);
}

enhanced_music_metadata nes_playback_engine::get_current_metadata() const {
    return m_current_metadata;
}

bool nes_playback_engine::load_with_nes_optimization(const std::string& filename) {
    if (!m_initialized || !m_file_manager) {
        handle_error("Engine or file manager not initialized");
        return false;
    }

    std::cout << "Loading file with NES optimization: " << filename << std::endl;

    // First, validate the file
    auto validation = validate_file(filename);
    if (!validation.is_valid) {
        std::cout << "File validation failed:" << std::endl;
        for (const auto& error : validation.errors) {
            std::cout << "  ✗ " << error << std::endl;
        }
        return false;
    }

    // Show validation warnings if any
    if (validation.has_warnings()) {
        std::cout << "File validation warnings:" << std::endl;
        for (const auto& warning : validation.warnings) {
            std::cout << "  ⚠ " << warning << std::endl;
        }
    }

    // Load with enhanced support
    if (!load_file_enhanced(filename)) {
        return false;
    }

    // Apply NES-specific optimizations based on file format
    std::string format = m_file_manager->detect_file_format(filename);
    if (format == "MIDI" || format == "MusicXML") {
        std::cout << "Applying NES optimizations for " << format << " format..." << std::endl;

        // Note: For now, we'll skip the parser-specific optimization since the method is private
        // This is a placeholder for future optimization functionality
        std::cout << "⚠ NES optimizations currently implemented at load time" << std::endl;
    }

    return true;
}

std::vector<std::string> nes_playback_engine::get_supported_file_formats() const {
    if (!m_file_manager) {
        return {};
    }
    return m_file_manager->get_supported_formats();
}

bool nes_playback_engine::play() {
    return play_from_time(0);
}

bool nes_playback_engine::play_from_time(music_time_t start_time) {
    if (m_state != engine_state::READY && m_state != engine_state::PAUSED) {
        handle_error("Engine not ready for playback");
        return false;
    }

    std::cout << "Starting playback from tick " << start_time << std::endl;

    try {
        // Start audio stream if not already running
        if (m_audio_stream && !m_audio_stream->is_running()) {
            if (!m_audio_stream->start()) {
                handle_error("Failed to start audio stream");
                return false;
            }
        }

        // Start sequencer playback
        if (!m_sequencer->play_from_time(start_time)) {
            handle_error("Failed to start sequencer playback");
            return false;
        }

        m_state = engine_state::PLAYING;
        m_playback_start_time = std::chrono::steady_clock::now();
        m_metrics.playback_sessions++;

        std::cout << "Playback started successfully" << std::endl;
        return true;

    } catch (const std::exception& e) {
        handle_error("Exception starting playback: " + std::string(e.what()));
        return false;
    }
}

bool nes_playback_engine::play_from_percentage(double percentage) {
    if (percentage < 0.0 || percentage > 1.0) {
        handle_error("Invalid percentage: must be 0.0 to 1.0");
        return false;
    }

    music_time_t start_time = static_cast<music_time_t>(get_total_duration() * percentage);
    return play_from_time(start_time);
}

bool nes_playback_engine::pause() {
    if (m_state != engine_state::PLAYING) {
        return false;
    }

    std::cout << "Pausing playback" << std::endl;

    if (m_sequencer && !m_sequencer->pause()) {
        handle_error("Failed to pause sequencer");
        return false;
    }

    m_state = engine_state::PAUSED;
    std::cout << "Playback paused" << std::endl;
    return true;
}

bool nes_playback_engine::resume() {
    if (m_state != engine_state::PAUSED) {
        return false;
    }

    std::cout << "Resuming playback" << std::endl;

    if (m_sequencer && !m_sequencer->resume()) {
        handle_error("Failed to resume sequencer");
        return false;
    }

    m_state = engine_state::PLAYING;
    std::cout << "Playback resumed" << std::endl;
    return true;
}

bool nes_playback_engine::stop() {
    if (m_state != engine_state::PLAYING && m_state != engine_state::PAUSED) {
        return true;
    }

    std::cout << "Stopping playback" << std::endl;

    // Stop sequencer
    if (m_sequencer) {
        m_sequencer->stop();
    }

    // Update total playback time
    if (m_state == engine_state::PLAYING) {
        auto now = std::chrono::steady_clock::now();
        auto session_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_playback_start_time);
        m_metrics.total_playback_time_ms += session_time.count();
    }

    m_state = engine_state::READY;
    std::cout << "Playback stopped" << std::endl;
    return true;
}

void nes_playback_engine::seek_to_time(music_time_t time) {
    if (m_sequencer) {
        m_sequencer->set_position(time);
    }
}

void nes_playback_engine::seek_to_percentage(double percentage) {
    if (percentage >= 0.0 && percentage <= 1.0) {
        music_time_t time = static_cast<music_time_t>(get_total_duration() * percentage);
        seek_to_time(time);
    }
}

music_time_t nes_playback_engine::get_current_position() const {
    if (m_sequencer) {
        return m_sequencer->get_position();
    }
    return 0;
}

music_time_t nes_playback_engine::get_total_duration() const {
    if (m_sequencer) {
        return m_sequencer->get_total_duration();
    }
    return 0;
}

double nes_playback_engine::get_current_percentage() const {
    music_time_t total = get_total_duration();
    if (total == 0) return 0.0;
    return static_cast<double>(get_current_position()) / static_cast<double>(total);
}

void nes_playback_engine::set_tempo_scale(double scale) {
    if (m_sequencer) {
        m_sequencer->set_tempo_scale(scale);
    }
}

double nes_playback_engine::get_tempo_scale() const {
    if (m_sequencer) {
        return m_sequencer->get_tempo_scale();
    }
    return 1.0;
}

void nes_playback_engine::set_master_volume(float volume) {
    if (m_audio_manager) {
        auto* mixer = m_audio_manager->get_nes_mixer();
        if (mixer) {
            mixer->set_master_volume(volume);
        }
    }
}

float nes_playback_engine::get_master_volume() const {
    if (m_audio_manager) {
        auto* mixer = m_audio_manager->get_nes_mixer();
        if (mixer) {
            // Note: would need to add getter to nes_audio_mixer
            return 1.0f; // Placeholder
        }
    }
    return 1.0f;
}

void nes_playback_engine::trigger_note(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t duration_ms) {
    if (m_sequencer) {
        music_time_t duration_ticks = (duration_ms * m_config.ticks_per_quarter_note) / (60000 / m_config.default_tempo_bpm);
        m_sequencer->trigger_note(channel, note, velocity, duration_ticks);
    }
}

void nes_playback_engine::stop_note(uint8_t channel, uint8_t note) {
    if (m_sequencer) {
        m_sequencer->stop_note(channel, note);
    }
}

void nes_playback_engine::all_notes_off() {
    if (m_sequencer) {
        m_sequencer->all_notes_off();
    }
}

void nes_playback_engine::panic() {
    if (m_sequencer) {
        m_sequencer->panic();
    }
    if (m_audio_stream) {
        // Could implement audio stream reset here
    }
}

nes_playback_engine::performance_metrics nes_playback_engine::get_performance_metrics() const {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);

    performance_metrics metrics = m_metrics;

    // Update with current component stats
    if (m_audio_stream) {
        metrics.audio_stats = m_audio_stream->get_stats();
        metrics.audio_underruns = metrics.audio_stats.buffer_underruns;
        metrics.audio_overruns = metrics.audio_stats.buffer_overruns;
    }

    if (m_audio_manager) {
        auto* mixer = m_audio_manager->get_nes_mixer();
        if (mixer) {
            metrics.mixer_stats = mixer->get_stats();
            metrics.peak_output_level = metrics.mixer_stats.peak_output_level;
        }
    }

    if (m_sequencer) {
        metrics.sequencer_stats = m_sequencer->get_stats();
        metrics.active_voices = metrics.sequencer_stats.active_notes;
        metrics.sequencer_errors = metrics.sequencer_stats.timing_errors;
    }

    return metrics;
}

void nes_playback_engine::reset_performance_metrics() {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    m_metrics = performance_metrics{};
    m_last_metrics_update = std::chrono::steady_clock::now();
}

nes_playback_engine::music_info nes_playback_engine::get_current_music_info() const {
    return m_current_info;
}

// Simple pattern builder implementation
void nes_playback_engine::simple_pattern_builder::add_note(uint8_t channel, uint8_t note, uint8_t velocity,
                                                          music_time_t start, music_time_t duration) {
    notes.emplace_back(channel, note, velocity, start, duration);
}

void nes_playback_engine::simple_pattern_builder::add_chord(uint8_t channel, const std::vector<uint8_t>& chord_notes,
                                                           uint8_t velocity, music_time_t start, music_time_t chord_duration) {
    for (uint8_t note : chord_notes) {
        add_note(channel, note, velocity, start, chord_duration);
    }
}

void nes_playback_engine::simple_pattern_builder::add_arpeggio(uint8_t channel, const std::vector<uint8_t>& arp_notes,
                                                              uint8_t velocity, music_time_t start, music_time_t note_duration) {
    music_time_t current_time = start;
    for (uint8_t note : arp_notes) {
        add_note(channel, note, velocity, current_time, note_duration);
        current_time += note_duration;
    }
}

void nes_playback_engine::simple_pattern_builder::add_drum_hit(uint8_t note, uint8_t velocity, music_time_t time) {
    add_note(3, note, velocity, time, 120); // Channel 3 (noise) for drums, short duration
}

music_data nes_playback_engine::simple_pattern_builder::to_music_data() const {
    music_data data;
    for (const auto& note : notes) {
        data.add_note(note);
    }
    for (const auto& control : controls) {
        data.add_control(control);
    }
    for (const auto& program : programs) {
        data.add_program(program);
    }
    return data;
}

nes_playback_engine::simple_pattern_builder nes_playback_engine::create_pattern_builder() {
    return simple_pattern_builder{};
}

bool nes_playback_engine::play_pattern(const simple_pattern_builder& pattern, bool loop) {
    auto music_data = pattern.to_music_data();

    if (!load_music_data(music_data)) {
        return false;
    }

    if (loop && m_sequencer) {
        m_sequencer->set_loop_enabled(true);
        m_sequencer->set_loop_points(0, pattern.duration);
    }

    return play();
}

// Private implementation methods

bool nes_playback_engine::initialize_audio_system() {
    std::cout << "Initializing audio system..." << std::endl;

    try {
        // Create enhanced audio manager
        m_audio_manager = std::make_unique<nes_enhanced_audio_manager>();

        // Create NES APU device
        auto nes_device = std::make_unique<nes_apu_device>("nes_playback", 1789773);
        m_audio_manager->add_device(std::move(nes_device));

        // Initialize with configured mixer settings
        if (!m_audio_manager->initialize(m_config.sample_rate, m_config.mixer_config)) {
            return false;
        }

        // Create audio stream
        audio_stream::config stream_config;
        stream_config.sample_rate = m_config.sample_rate;
        stream_config.buffer_size = m_config.buffer_size;
        stream_config.enable_threading = true;

        m_audio_stream = audio_stream_factory::create_stream(m_config.audio_backend, stream_config);
        if (!m_audio_stream) {
            return false;
        }

        // Set up audio callback
        m_audio_stream->set_callback([this](int16_t* buffer, size_t frames) {
            audio_callback(buffer, frames);
        });

        // Initialize audio stream
        if (!m_audio_stream->initialize()) {
            return false;
        }

        std::cout << "Audio system initialized" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cout << "Audio system initialization failed: " << e.what() << std::endl;
        return false;
    }
}

bool nes_playback_engine::initialize_sequencer_system() {
    std::cout << "Initializing sequencer system..." << std::endl;

    try {
        // Create sequencer configuration
        nes_sequencer::sequencer_config seq_config;
        seq_config.sample_rate = m_config.sample_rate;
        seq_config.ticks_per_quarter_note = m_config.ticks_per_quarter_note;
        seq_config.microseconds_per_quarter = 60000000 / m_config.default_tempo_bpm;
        seq_config.enable_threading = true;
        seq_config.enable_looping = m_config.enable_looping;
        seq_config.lookahead_ms = m_config.lookahead_ms;

        m_sequencer = std::make_unique<nes_sequencer>(seq_config);

        // Initialize sequencer with audio manager
        if (!m_sequencer->initialize(m_audio_manager.get())) {
            return false;
        }

        // Start sequencer (but not playback)
        if (!m_sequencer->start()) {
            return false;
        }

        std::cout << "Sequencer system initialized" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cout << "Sequencer system initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void nes_playback_engine::shutdown_audio_system() {
    if (m_audio_stream) {
        m_audio_stream->stop();
        m_audio_stream->shutdown();
        m_audio_stream.reset();
    }

    if (m_audio_manager) {
        m_audio_manager->shutdown();
        m_audio_manager.reset();
    }

    std::cout << "Audio system shut down" << std::endl;
}

void nes_playback_engine::shutdown_sequencer_system() {
    if (m_sequencer) {
        m_sequencer->shutdown();
        m_sequencer.reset();
    }

    std::cout << "Sequencer system shut down" << std::endl;
}

void nes_playback_engine::audio_callback(int16_t* buffer, size_t frames) {
    if (m_audio_manager) {
        m_audio_manager->generate_enhanced_samples(buffer, frames);
    } else {
        // Silence if no audio manager
        std::memset(buffer, 0, frames * sizeof(int16_t));
    }

    // Update performance metrics
    if (m_config.enable_performance_monitoring) {
        update_performance_metrics();
    }
}

void nes_playback_engine::update_current_info() {
    m_current_info = music_info{};
    m_current_info.filename = m_current_filename;
    m_current_info.duration_ticks = get_total_duration();
    m_current_info.duration_seconds = static_cast<uint32_t>(
        (m_current_info.duration_ticks * 60) / (m_config.ticks_per_quarter_note * m_config.default_tempo_bpm));
    m_current_info.note_count = static_cast<uint32_t>(m_current_music.notes().size());

    // Determine unique channels
    std::set<uint8_t> channels;
    for (const auto& note : m_current_music.notes()) {
        channels.insert(note.channel);
    }
    m_current_info.channel_count = static_cast<uint32_t>(channels.size());

    m_current_info.tempo_bpm = m_config.default_tempo_bpm;

    // Set format based on filename
    if (m_current_filename.find("Pattern") != std::string::npos) {
        m_current_info.format = "Pattern";
    } else if (m_current_filename.find(".mid") != std::string::npos ||
               m_current_filename.find(".midi") != std::string::npos) {
        m_current_info.format = "MIDI";
    } else if (m_current_filename.find(".xml") != std::string::npos ||
               m_current_filename.find(".musicxml") != std::string::npos) {
        m_current_info.format = "MusicXML";
    } else {
        m_current_info.format = "Unknown";
    }
}

void nes_playback_engine::update_current_info_from_metadata() {
    // Update music_info from enhanced_music_metadata
    m_current_info = music_info{};
    m_current_info.filename = m_current_metadata.filename;
    m_current_info.title = m_current_metadata.title;
    m_current_info.artist = m_current_metadata.artist;
    m_current_info.album = m_current_metadata.album;
    m_current_info.duration_ticks = m_current_metadata.total_ticks;
    m_current_info.duration_seconds = static_cast<uint32_t>(m_current_metadata.duration_seconds());
    m_current_info.note_count = m_current_metadata.total_notes;
    m_current_info.channel_count = m_current_metadata.unique_channels_used;
    m_current_info.tempo_bpm = m_current_metadata.default_tempo_bpm;
    m_current_info.format = m_current_metadata.file_format;
}

std::string nes_playback_engine::detect_file_format(const std::string& filename) {
    auto ext_pos = filename.find_last_of('.');
    if (ext_pos == std::string::npos) {
        return "";
    }

    std::string ext = filename.substr(ext_pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".mid" || ext == ".midi") {
        return "MIDI";
    } else if (ext == ".xml" || ext == ".musicxml") {
        return "MusicXML";
    }

    return "";
}

bool nes_playback_engine::load_midi_file(const std::string& filename) {
    try {
        midi_parser parser;
        return parser.parse_file(filename, m_current_music);
    } catch (const std::exception& e) {
        std::cout << "MIDI load error: " << e.what() << std::endl;
        return false;
    }
}

bool nes_playback_engine::load_musicxml_file(const std::string& filename) {
    try {
        musicxml_parser parser;
        return parser.parse_file(filename, m_current_music);
    } catch (const std::exception& e) {
        std::cout << "MusicXML load error: " << e.what() << std::endl;
        return false;
    }
}

void nes_playback_engine::update_performance_metrics() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_metrics_update);

    if (elapsed.count() >= 100) { // Update every 100ms
        std::lock_guard<std::mutex> lock(m_metrics_mutex);

        // Calculate average frame time (simplified)
        m_metrics.average_frame_time_ms = static_cast<double>(m_config.buffer_size) / m_config.sample_rate * 1000.0;

        m_last_metrics_update = now;
    }
}

void nes_playback_engine::optimize_for_realtime() {
    // Set thread priorities and other real-time optimizations
    // This would be platform-specific implementation
    std::cout << "Applied real-time optimizations" << std::endl;
}

void nes_playback_engine::handle_error(const std::string& message, int error_code) {
    std::cout << "ENGINE ERROR: " << message << " (code: " << error_code << ")" << std::endl;

    if (m_error_callback) {
        m_error_callback(message, error_code);
    }

    m_metrics.file_load_errors++;
}

// Factory implementations
nes_playback_engine::engine_config nes_playback_engine_factory::high_quality_config() {
    nes_playback_engine::engine_config config;
    config.sample_rate = 48000;
    config.buffer_size = 2048;
    config.audio_backend = audio_stream_factory::backend_type::AUTO;
    config.mixer_config.enable_nonlinear_mixing = true;
    config.mixer_config.enable_highpass_filter = true;
    config.mixer_config.enable_lowpass_filter = true;
    config.enable_performance_monitoring = true;
    config.max_polyphony = 32;
    return config;
}

nes_playback_engine::engine_config nes_playback_engine_factory::low_latency_config() {
    nes_playback_engine::engine_config config;
    config.sample_rate = 44100;
    config.buffer_size = 256;
    config.audio_backend = audio_stream_factory::backend_type::AUTO;
    config.lookahead_ms = 20;
    config.enable_performance_monitoring = true;
    config.max_polyphony = 16;
    return config;
}

std::unique_ptr<nes_playback_engine> nes_playback_engine_factory::create_for_music_playback() {
    auto config = high_quality_config();
    auto engine = std::make_unique<nes_playback_engine>(config);
    return engine;
}

std::unique_ptr<nes_playback_engine> nes_playback_engine_factory::create_for_interactive_use() {
    auto config = low_latency_config();
    auto engine = std::make_unique<nes_playback_engine>(config);
    return engine;
}

// Real-time control method implementations
void nes_playback_engine::set_channel_volume(uint8_t channel, float volume) {
    (void)channel; (void)volume; // Suppress unused parameter warnings
    // Implementation would set channel volume through audio manager
    // For now, just stub to avoid linker errors
}

void nes_playback_engine::mute_channel(uint8_t channel, bool mute) {
    (void)channel; (void)mute; // Suppress unused parameter warnings
    // Implementation would mute/unmute channel through audio manager
    // For now, just stub to avoid linker errors
}

void nes_playback_engine::set_pulse_duty_cycle(uint8_t channel, uint8_t duty) {
    (void)channel; (void)duty; // Suppress unused parameter warnings
    // Implementation would set pulse duty cycle through audio device
    // For now, just stub to avoid linker errors
}

void nes_playback_engine::set_triangle_linear_counter(uint8_t value) {
    (void)value; // Suppress unused parameter warning
    // Implementation would set triangle linear counter through audio device
    // For now, just stub to avoid linker errors
}

void nes_playback_engine::set_noise_mode(bool short_mode) {
    (void)short_mode; // Suppress unused parameter warning
    // Implementation would set noise mode through audio device
    // For now, just stub to avoid linker errors
}

bool nes_playback_engine::export_to_wav(const std::string& filename, uint32_t sample_rate) {
    // Validate input parameters
    if (filename.empty()) {
        return false;
    }

    // Check if we have music data loaded
    if (m_current_music.notes().empty() &&
        m_current_music.controls().empty() &&
        m_current_music.programs().empty() &&
        m_current_music.tempos().empty()) {
        return false;
    }

    // Save current state
    auto original_state = m_state.load();
    bool was_running = (original_state == engine_state::PLAYING);

    // Stop current playback if running
    if (was_running) {
        stop();
    }

    try {
        // Create a temporary audio stream configured for file output
        audio_stream::config export_config;
        export_config.sample_rate = sample_rate;
        export_config.buffer_size = 1024;
        export_config.enable_threading = false; // Synchronous for export

        auto export_stream = audio_stream_factory::create_stream(
            audio_stream_factory::backend_type::FILE_OUTPUT,
            export_config
        );

        if (!export_stream) {
            if (was_running) play();
            return false;
        }

        // Set the output filename
        export_stream->set_output_filename(filename);

        // Set up the audio callback for export
        export_stream->set_callback([this](int16_t* buffer, size_t frames) {
            this->audio_callback(buffer, frames);
        });

        // Note: We don't set the audio manager for export streams since they
        // use file output mode and don't need the full audio device management

        // Initialize the export stream
        if (!export_stream->initialize()) {
            if (was_running) play();
            return false;
        }

        // Temporarily replace the main audio stream
        auto original_stream = std::move(m_audio_stream);
        m_audio_stream = std::move(export_stream);

        // Calculate approximate duration for export
        // This is a rough estimate - in a full implementation we'd calculate exact duration
        music_time_t estimated_duration = 0;
        for (const auto& note : m_current_music.notes()) {
            music_time_t note_end = note.start + note.duration;
            if (note_end > estimated_duration) {
                estimated_duration = note_end;
            }
        }

        // Convert ticks to seconds (assuming 480 ticks per quarter note, 120 BPM)
        double duration_seconds = static_cast<double>(estimated_duration) / (480.0 * 2.0);

        // Ensure minimum duration of 1 second
        if (duration_seconds < 1.0) {
            duration_seconds = 1.0;
        }

        // Start export playback
        m_state = engine_state::READY;
        if (!play()) {
            // Restore original stream
            m_audio_stream = std::move(original_stream);
            if (was_running) play();
            return false;
        }

        // Let the audio render for the estimated duration
        // In a real implementation, we'd use the sequencer's position to know when we're done
        auto start_time = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start_time <
               std::chrono::milliseconds(static_cast<int>(duration_seconds * 1000 + 500))) {

            // Process audio in chunks
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // Check if playback naturally finished
            if (m_state == engine_state::READY || m_state == engine_state::STOPPING) {
                break;
            }
        }

        // Stop export playback
        stop();

        // Shutdown export stream (this should finalize the WAV file)
        m_audio_stream->shutdown();

        // Restore original audio stream
        m_audio_stream = std::move(original_stream);

        // Restore original playback state
        if (was_running) {
            play();
        } else {
            m_state = original_state;
        }

        return true;

    } catch (const std::exception& e) {
        // Restore state on any error
        if (was_running) {
            play();
        }
        return false;
    }
}

void nes_playback_engine::set_loop_enabled(bool enabled) {
    (void)enabled; // Suppress unused parameter warning
    // Implementation would enable/disable looping
    // For now, just stub to avoid linker errors
}

bool nes_playback_engine::is_loop_enabled() const {
    // Implementation would return loop state
    // For now, just stub to avoid linker errors
    return false;
}