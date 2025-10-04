#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

/**
 * Debug Configuration System for MIDI-to-WAV Conversion Debugging
 *
 * This system provides centralized control over debug logging throughout
 * the MAME synth codebase, enabling systematic debugging of conversion issues.
 */

struct debug_config {
    // MIDI Event Processing
    bool log_midi_events = false;          // Log MIDI note on/off events with timestamps
    bool log_midi_timing = false;          // Log MIDI timing calculations and sequencer state
    bool log_midi_parsing = false;         // Log MIDI file parsing details

    // NES Register Operations
    bool log_register_writes = false;      // Log all NES APU register writes
    bool log_frequency_calc = false;       // Log frequency-to-timer calculations
    bool log_channel_state = false;        // Log NES channel state changes

    // Audio System
    bool log_audio_buffers = false;        // Log audio buffer fill status and timing
    bool log_audio_stream = false;         // Log audio stream operations
    bool log_audio_mixing = false;         // Log NES audio mixing operations

    // Timing and Sequencing
    bool log_timing_info = false;          // Log sequencer timing and synchronization
    bool log_playback_engine = false;      // Log playback engine state transitions
    bool log_export_process = true;        // Log WAV export process details

    // File Operations
    bool log_wav_export = false;           // Log WAV file export operations
    bool log_file_operations = false;      // Log file I/O operations

    // Debug Output Control
    bool export_debug_wav = false;         // Export debug WAV files during processing
    bool save_debug_logs = false;          // Save logs to files
    std::string debug_log_prefix = "debug_"; // Prefix for debug log files

    // Performance Monitoring
    bool log_performance = false;          // Log performance metrics
    bool log_memory_usage = false;         // Log memory allocation/deallocation

    // Integration Testing
    bool log_test_operations = false;      // Log test framework operations
};

/**
 * Debug Logger - Thread-safe logging with timestamps and categories
 */
class debug_logger {
public:
    enum class level {
        INFO,
        DEBUG,
        WARNING,
        ERROR,
        TIMING,
        MIDI,
        AUDIO,
        REGISTER
    };

private:
    static debug_config s_config;
    static std::ofstream s_log_file;
    static bool s_file_logging_enabled;
    static std::string get_timestamp();
    static std::string level_to_string(level lvl);

public:
    static void set_config(const debug_config& config);
    static const debug_config& get_config();
    static void enable_file_logging(const std::string& filename = "");
    static void disable_file_logging();

    // Main logging functions
    static void log(level lvl, const std::string& category, const std::string& message);
    static void log_midi_event(const std::string& message);
    static void log_register_write(uint32_t offset, uint8_t value, const std::string& description = "");
    static void log_audio_buffer(const std::string& message);
    static void log_timing(const std::string& message);
    static void log_performance(const std::string& operation, double duration_ms);

    // Convenience macros will be defined after class declaration
};

// Global debug configuration instance
extern debug_config g_debug_config;

// Convenience macros for logging (only active when corresponding debug flags are set)
#define DEBUG_LOG_MIDI(msg) \
    do { if (g_debug_config.log_midi_events) debug_logger::log_midi_event(msg); } while(0)

#define DEBUG_LOG_REGISTER(offset, value, desc) \
    do { if (g_debug_config.log_register_writes) debug_logger::log_register_write(offset, value, desc); } while(0)

#define DEBUG_LOG_AUDIO(msg) \
    do { if (g_debug_config.log_audio_buffers) debug_logger::log_audio_buffer(msg); } while(0)

#define DEBUG_LOG_TIMING(msg) \
    do { if (g_debug_config.log_timing_info) debug_logger::log_timing(msg); } while(0)

#define DEBUG_LOG_PERFORMANCE(op, duration) \
    do { if (g_debug_config.log_performance) debug_logger::log_performance(op, duration); } while(0)

#define DEBUG_LOG_INFO(category, msg) \
    do { debug_logger::log(debug_logger::level::INFO, category, msg); } while(0)

#define DEBUG_LOG_ERROR(category, msg) \
    do { debug_logger::log(debug_logger::level::ERROR, category, msg); } while(0)

#define DEBUG_LOG_WARNING(category, msg) \
    do { debug_logger::log(debug_logger::level::WARNING, category, msg); } while(0)

// Performance timing helper class
class debug_timer {
private:
    std::chrono::steady_clock::time_point m_start;
    std::string m_operation;

public:
    debug_timer(const std::string& operation) : m_operation(operation) {
        m_start = std::chrono::steady_clock::now();
    }

    ~debug_timer() {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start);
        double duration_ms = static_cast<double>(duration.count()) / 1000.0;
        DEBUG_LOG_PERFORMANCE(m_operation, duration_ms);
    }
};

#define DEBUG_TIMER(operation) debug_timer __timer(operation)

// Debug configuration presets for common debugging scenarios
namespace debug_presets {
    // Preset for debugging MIDI event processing issues
    inline debug_config midi_debugging() {
        debug_config config = {};
        config.log_midi_events = true;
        config.log_midi_timing = true;
        config.log_midi_parsing = true;
        config.log_timing_info = true;
        config.log_playback_engine = true;
        return config;
    }

    // Preset for debugging audio generation issues
    inline debug_config audio_debugging() {
        debug_config config = {};
        config.log_register_writes = true;
        config.log_frequency_calc = true;
        config.log_channel_state = true;
        config.log_audio_buffers = true;
        config.log_audio_mixing = true;
        config.export_debug_wav = true;
        return config;
    }

    // Preset for debugging WAV export issues
    inline debug_config export_debugging() {
        debug_config config = {};
        config.log_wav_export = true;
        config.log_export_process = true;
        config.log_file_operations = true;
        config.log_audio_stream = true;
        config.save_debug_logs = true;
        return config;
    }

    // Preset for comprehensive debugging (all systems)
    inline debug_config comprehensive_debugging() {
        debug_config config = {};
        config.log_midi_events = true;
        config.log_midi_timing = true;
        config.log_register_writes = true;
        config.log_frequency_calc = true;
        config.log_channel_state = true;
        config.log_audio_buffers = true;
        config.log_timing_info = true;
        config.log_playback_engine = true;
        config.log_wav_export = true;
        config.log_export_process = true;
        config.export_debug_wav = true;
        config.save_debug_logs = true;
        config.log_performance = true;
        return config;
    }

    // Minimal debugging for production testing
    inline debug_config minimal_debugging() {
        debug_config config = {};
        config.log_midi_events = true;
        config.log_register_writes = true;
        config.log_wav_export = true;
        return config;
    }
}