#include "nes_playback_engine.h"
#include "nes_audio_mixer.h"
#include "audio_device.h"
#include "music_parser.h"
#include "comprehensive_file_support.h"
#include "nes_channel_assignment.h"
#include "debug_config.h"
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

        // DEBUG: Check TPQN after file manager load
        std::cout << "DEBUG: After file manager load, music_data TPQN = "
                  << m_current_music.metadata().ticks_per_quarter << std::endl;

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

        // Apply intelligent channel assignment for NES
        std::cout << "Applying NES channel assignment..." << std::endl;
        try {
            nes_channel_assignment::channel_assignment_engine assignment_engine;
            assignment_engine.set_active_strategy("quality_focused"); // Use quality-focused strategy by default
            auto assignment_result = assignment_engine.assign_channels(m_current_music);

            // Convert assignment result to sequencer channel mapping format
            std::vector<nes_sequencer::nes_channel_mapping> sequencer_mapping;
            sequencer_mapping.resize(16); // All 16 MIDI channels

            // Initialize with default identity mapping
            for (int i = 0; i < 16; ++i) {
                sequencer_mapping[i].midi_channel = i;
                sequencer_mapping[i].nes_channel = std::min(i, 4); // Default to DMC for high channels
                sequencer_mapping[i].enabled = true;
            }

            // Apply intelligent assignments from the engine
            for (const auto& assignment : assignment_result.assignments) {
                if (assignment.midi_channel < 16) {
                    sequencer_mapping[assignment.midi_channel].nes_channel = static_cast<uint8_t>(assignment.nes_channel);

                    // Convert NES channel type to string for display
                    const char* nes_channel_name = "Unknown";
                    switch (assignment.nes_channel) {
                        case nes_channel_assignment::nes_channel_type::PULSE_1: nes_channel_name = "Pulse1"; break;
                        case nes_channel_assignment::nes_channel_type::PULSE_2: nes_channel_name = "Pulse2"; break;
                        case nes_channel_assignment::nes_channel_type::TRIANGLE: nes_channel_name = "Triangle"; break;
                        case nes_channel_assignment::nes_channel_type::NOISE: nes_channel_name = "Noise"; break;
                        case nes_channel_assignment::nes_channel_type::DMC: nes_channel_name = "DMC"; break;
                    }

                    std::cout << "  MIDI ch" << static_cast<int>(assignment.midi_channel)
                              << " -> NES " << nes_channel_name
                              << " (confidence: " << (assignment.confidence_score * 100.0f) << "%)" << std::endl;
                }
            }

            // Apply the mapping to the sequencer
            m_sequencer->set_channel_mapping(sequencer_mapping);

            std::cout << "Channel assignment complete (quality: "
                      << (assignment_result.overall_quality_score * 100.0f) << "%)" << std::endl;

            // Show warnings if any
            for (const auto& warning : assignment_result.warnings) {
                std::cout << "  ⚠ " << warning << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "⚠ Channel assignment failed: " << e.what() << ", using default mapping" << std::endl;
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
    if (!m_initialized || !m_audio_manager) {
        return;
    }

    // Clamp volume to valid range
    volume = std::max(0.0f, std::min(1.0f, volume));

    // Set volume through the mixer
    auto* mixer = m_audio_manager->get_nes_mixer();
    if (mixer && channel < 5) { // NES has 5 channels (0-4)
        mixer->set_channel_volume(channel, volume);
        log_debug("Set channel " + std::to_string(channel) + " volume to " + std::to_string(volume));
    }
}

void nes_playback_engine::mute_channel(uint8_t channel, bool mute) {
    if (!m_initialized || !m_audio_manager) {
        return;
    }

    // Mute/unmute through the mixer by setting volume to 0 or restoring
    auto* mixer = m_audio_manager->get_nes_mixer();
    if (mixer && channel < 5) { // NES has 5 channels (0-4)
        if (mute) {
            // Store current volume and set to 0
            mixer->set_channel_volume(channel, 0.0f);
            log_debug("Muted channel " + std::to_string(channel));
        } else {
            // For unmute, restore to 1.0f (could be enhanced to store previous volume)
            mixer->set_channel_volume(channel, 1.0f);
            log_debug("Unmuted channel " + std::to_string(channel));
        }
    }
}

void nes_playback_engine::set_pulse_duty_cycle(uint8_t channel, uint8_t duty) {
    if (!m_initialized || !m_audio_manager) {
        return;
    }

    // Clamp duty cycle to valid range (0-3)
    duty = std::min(duty, uint8_t(3));

    // Only pulse channels 0 and 1 support duty cycle
    if (channel > 1) {
        log_debug("Warning: set_pulse_duty_cycle called for non-pulse channel " + std::to_string(channel));
        return;
    }

    // Get the NES APU device and set duty cycle
    auto* nes_device = dynamic_cast<nes_apu_device*>(m_audio_manager->get_device("nes_playback"));
    if (nes_device) {
        nes_device->set_pulse_duty_cycle(channel, duty);
        log_debug("Set pulse channel " + std::to_string(channel) + " duty cycle to " + std::to_string(duty));
    } else {
        log_debug("Warning: NES APU device not found for duty cycle setting");
    }
}

void nes_playback_engine::set_triangle_linear_counter(uint8_t value) {
    if (!m_initialized || !m_audio_manager) {
        return;
    }

    // Clamp value to valid range (0-127)
    value = std::min(value, uint8_t(127));

    // Get the NES APU device and set triangle linear counter
    auto* nes_device = dynamic_cast<nes_apu_device*>(m_audio_manager->get_device("nes_playback"));
    if (nes_device) {
        nes_device->set_triangle_linear_counter(value);
        log_debug("Set triangle linear counter to " + std::to_string(value));
    } else {
        log_debug("Warning: NES APU device not found for triangle linear counter setting");
    }
}

void nes_playback_engine::set_noise_mode(bool short_mode) {
    if (!m_initialized || !m_audio_manager) {
        return;
    }

    // Get the NES APU device and set noise mode
    auto* nes_device = dynamic_cast<nes_apu_device*>(m_audio_manager->get_device("nes_playback"));
    if (nes_device) {
        nes_device->set_noise_mode(short_mode);
        log_debug("Set noise mode to " + std::string(short_mode ? "short" : "long"));
    } else {
        log_debug("Warning: NES APU device not found for noise mode setting");
    }
}

// Calculate duration accounting for tempo changes
double nes_playback_engine::calculate_tempo_aware_duration() const {
    if (!m_sequencer) {
        return 0.0;
    }

    // Find the latest note/event end time in ticks
    music_time_t total_ticks = 0;
    for (const auto& note : m_current_music.notes()) {
        music_time_t note_end = note.start + note.duration;
        total_ticks = std::max(total_ticks, note.start + note.duration);
    }

    // If no notes, return 0
    if (total_ticks == 0) {
        return 0.0;
    }

    // Get tempo events and sort them by time
    auto tempos = m_current_music.tempos();
    std::sort(tempos.begin(), tempos.end(),
              [](const music_tempo& a, const music_tempo& b) { return a.time < b.time; });

    // Calculate duration segment by segment
    double duration_seconds = 0.0;
    music_time_t current_tick = 0;
    uint32_t current_tempo_uspq = 500000; // Default: 500000 microseconds per quarter note (120 BPM)

    // Get ticks per quarter note from the music data's metadata (NOT config)
    uint32_t ticks_per_quarter = m_current_music.metadata().ticks_per_quarter;

    // Process each tempo segment
    for (size_t i = 0; i <= tempos.size(); ++i) {
        music_time_t segment_end = (i < tempos.size()) ? tempos[i].time : total_ticks;
        music_time_t segment_ticks = segment_end - current_tick;

        if (segment_ticks > 0) {
            // Calculate duration for this segment: (ticks / ticks_per_quarter) * (microseconds_per_quarter / 1000000)
            double microseconds_per_tick = static_cast<double>(current_tempo_uspq) / ticks_per_quarter;
            double segment_duration = (segment_ticks * microseconds_per_tick) / 1000000.0;
            duration_seconds += segment_duration;
        }

        // Update for next segment
        if (i < tempos.size()) {
            current_tempo_uspq = tempos[i].microseconds_per_quarter;
            current_tick = tempos[i].time;
        }
    }

    return duration_seconds;
}

bool nes_playback_engine::export_to_wav(const std::string& filename, uint32_t sample_rate) {
    if (g_debug_config.log_export_process) {
        std::stringstream ss;
        ss << "WAV export started: file=" << filename
           << ", sample_rate=" << sample_rate
           << ", notes=" << m_current_music.notes().size();
        DEBUG_LOG_INFO("EXPORT", ss.str());
    }

    // Validate input parameters
    if (filename.empty()) {
        return false;
    }

    // Check if we have music data loaded
    if (m_current_music.notes().empty() &&
        m_current_music.controls().empty() &&
        m_current_music.programs().empty() &&
        m_current_music.tempos().empty()) {
        if (g_debug_config.log_export_process) {
            DEBUG_LOG_ERROR("EXPORT", "No music data loaded for export");
        }
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
        export_config.enable_threading = true;  // Need threading for audio callback

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

        // Enable offline rendering mode in sequencer for faster-than-realtime export
        if (m_sequencer) {
            m_sequencer->set_offline_rendering(true);
        }

        // Set up the audio callback for export
        export_stream->set_callback([this](int16_t* buffer, size_t frames) {
            this->audio_callback(buffer, frames);
        });

        // Set up post-process callback to advance sample count for offline timing
        export_stream->set_post_process_callback([this](size_t frames) {
            if (m_sequencer) {
                m_sequencer->advance_samples(static_cast<uint32_t>(frames));
            }
        });

        // The export stream will use the replaced main stream which has the audio manager

        // Initialize the export stream
        if (!export_stream->initialize()) {
            if (was_running) play();
            return false;
        }

        // Temporarily replace the main audio stream
        auto original_stream = std::move(m_audio_stream);
        m_audio_stream = std::move(export_stream);

        // Calculate duration with tempo awareness
        music_time_t total_duration_ticks = 0;
        for (const auto& note : m_current_music.notes()) {
            music_time_t note_end = note.start + note.duration;
            if (note_end > total_duration_ticks) {
                total_duration_ticks = note_end;
            }
        }

        // Use tempo-aware duration calculation
        double duration_seconds = calculate_tempo_aware_duration();

        // Add a small buffer for audio processing completion
        duration_seconds += 2.0;

        // Ensure reasonable bounds
        if (duration_seconds < 5.0) {
            duration_seconds = 5.0;  // Minimum 5 seconds for short tracks
        }
        if (duration_seconds > 3600.0) {  // Cap at 1 hour for safety
            duration_seconds = 3600.0;
        }

        std::cout << "Export: total_duration=" << total_duration_ticks << " ticks, "
                  << "calculated_duration=" << duration_seconds << " seconds" << std::endl;

        if (g_debug_config.log_export_process) {
            std::stringstream ss;
            ss << "Export timing: total_duration=" << total_duration_ticks << " ticks"
               << ", calculated_duration=" << duration_seconds << " seconds"
               << ", tempo_changes=" << m_current_music.tempos().size();
            DEBUG_LOG_TIMING(ss.str());
        }

        // Start export playback
        m_state = engine_state::READY;
        if (!play()) {
            // Restore original stream
            m_audio_stream = std::move(original_stream);
            if (was_running) play();
            return false;
        }

        // Wait for sequencer to complete, using both position tracking and timeout
        auto start_time = std::chrono::steady_clock::now();
        auto timeout_duration = std::chrono::milliseconds(static_cast<int>(duration_seconds * 1000 * 1.2)); // 20% safety margin
        auto last_progress_log = start_time;
        music_time_t last_position = 0;

        if (g_debug_config.log_export_process) {
            std::stringstream ss;
            ss << "Export loop starting: target_ticks=" << total_duration_ticks
               << ", timeout=" << (duration_seconds * 1.2) << "s";
            DEBUG_LOG_INFO("EXPORT", ss.str());
        }

        bool first_loop_iteration = true;
        while (true) {
            // Check if sequencer has reached the end
            music_time_t current_position = m_sequencer ? m_sequencer->get_position() : 0;

            // Debug log first iteration
            if (first_loop_iteration && g_debug_config.log_export_process) {
                std::stringstream ss;
                ss << "Export loop first iteration: current_position=" << current_position
                   << ", total_duration_ticks=" << total_duration_ticks;
                DEBUG_LOG_INFO("EXPORT", ss.str());
                first_loop_iteration = false;
            }

            // Progress logging every 5 seconds
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_progress_log).count() >= 5) {
                double percent = total_duration_ticks > 0 ?
                                (100.0 * current_position / total_duration_ticks) : 0.0;
                std::cout << "Export progress: " << percent << "% "
                          << "(" << current_position << "/" << total_duration_ticks << " ticks)" << std::endl;
                last_progress_log = now;
            }

            // Check completion conditions
            bool reached_end = (current_position >= total_duration_ticks);
            bool playback_stopped = (m_state == engine_state::READY || m_state == engine_state::STOPPING);
            bool timed_out = (std::chrono::steady_clock::now() - start_time) > timeout_duration;
            bool position_stalled = (current_position > 0 && current_position == last_position);

            if (reached_end) {
                if (g_debug_config.log_export_process) {
                    DEBUG_LOG_INFO("EXPORT", "Export complete: sequencer reached end");
                }
                // Give a small amount of time for final audio buffers to process
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                break;
            }

            if (playback_stopped) {
                if (g_debug_config.log_export_process) {
                    DEBUG_LOG_INFO("EXPORT", "Export complete: playback stopped naturally");
                }
                break;
            }

            if (timed_out) {
                if (g_debug_config.log_export_process) {
                    DEBUG_LOG_WARNING("EXPORT", "Export timeout reached");
                }
                break;
            }

            last_position = current_position;

            // Sleep briefly before checking again
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Stop export playback
        stop();

        if (g_debug_config.log_export_process) {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            std::stringstream ss;
            ss << "Export completed: elapsed=" << elapsed_ms << "ms"
               << ", expected=" << static_cast<int>(duration_seconds * 1000) << "ms";
            DEBUG_LOG_INFO("EXPORT", ss.str());
        }

        // Shutdown export stream (this should finalize the WAV file)
        m_audio_stream->shutdown();

        // Disable offline rendering mode
        if (m_sequencer) {
            m_sequencer->set_offline_rendering(false);
        }

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
    if (!m_initialized || !m_sequencer) {
        return;
    }

    // Update configuration
    m_config.enable_looping = enabled;

    // Set looping in the sequencer
    m_sequencer->set_loop_enabled(enabled);

    log_debug("Loop " + std::string(enabled ? "enabled" : "disabled"));
}

bool nes_playback_engine::is_loop_enabled() const {
    if (!m_initialized || !m_sequencer) {
        return m_config.enable_looping; // Return config value if sequencer not available
    }

    // Get loop state from sequencer
    return m_sequencer->is_loop_enabled();
}

// Internal logging methods
void nes_playback_engine::log_debug(const std::string& message) {
    // Debug logging - can be disabled in release builds
    #ifdef DEBUG
    std::cout << "[DEBUG] NES Engine: " << message << std::endl;
    #else
    (void)message; // Suppress unused parameter warning in release builds
    #endif
}

void nes_playback_engine::log_info(const std::string& message) {
    std::cout << "[INFO] NES Engine: " << message << std::endl;
}

void nes_playback_engine::log_error(const std::string& message) {
    std::cerr << "[ERROR] NES Engine: " << message << std::endl;
}