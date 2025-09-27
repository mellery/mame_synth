#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <new>
#include <memory>

// Forward declarations
class nes_playback_engine;
class nes_cli;

/**
 * Comprehensive NES Configuration System
 *
 * Provides configuration management for all NES-specific features including:
 * - Hardware emulation accuracy settings
 * - Audio processing parameters
 * - Channel-specific configurations
 * - Performance optimization options
 * - File format handling preferences
 * - User interface customization
 */

namespace nes_config {

/**
 * NES Hardware Configuration - Controls emulation accuracy and behavior
 */
struct nes_hardware_config {
    // CPU and timing settings
    uint32_t cpu_clock_rate = 1789773;        // NTSC NES CPU frequency
    bool enable_cycle_accurate_timing = true;  // Precise cycle timing vs. simplified
    bool enable_frame_accurate_timing = true; // Frame-perfect timing for audio

    // APU configuration
    bool enable_apu_noise_randomization = true;   // Randomize noise channel on reset
    bool enable_length_counter_halt = true;      // Hardware-accurate length counters
    bool enable_envelope_looping = true;         // Envelope looping behavior
    bool enable_sweep_units = true;              // Enable frequency sweep units

    // Memory and cartridge simulation
    bool enable_cartridge_timing = false;       // Simulate cartridge read delays
    bool enable_mapper_quirks = false;          // Enable mapper-specific behaviors
    uint32_t cartridge_delay_cycles = 2;        // Cycles for cartridge access

    // Regional settings
    enum class region_type {
        NTSC,       // North American/Japanese NES (60Hz)
        PAL,        // European NES (50Hz)
        DENDY       // Russian Dendy variant
    };
    region_type region = region_type::NTSC;
};

/**
 * NES Audio Processing Configuration
 */
struct nes_audio_config {
    // Basic audio settings
    uint32_t sample_rate = 44100;
    uint32_t buffer_size = 1024;

    // Hardware-accurate mixing
    bool enable_nonlinear_mixing = true;        // Use NES DAC curves
    bool enable_dc_blocking = true;             // Remove DC offset
    bool enable_anti_aliasing = false;          // Software anti-aliasing filter

    // NES-specific filtering
    bool enable_highpass_filter = true;         // 90Hz highpass (real NES)
    bool enable_lowpass_filter = true;          // 14kHz lowpass (real NES)
    float highpass_cutoff_hz = 90.0f;          // Configurable highpass frequency
    float lowpass_cutoff_hz = 14000.0f;        // Configurable lowpass frequency

    // Channel volume scaling (hardware-accurate defaults)
    float pulse1_volume_scale = 1.0f;
    float pulse2_volume_scale = 1.0f;
    float triangle_volume_scale = 0.9f;         // Slightly quieter (hardware accurate)
    float noise_volume_scale = 0.7f;           // Quieter than pulse channels
    float dmc_volume_scale = 0.5f;             // Quietest channel

    // Advanced audio options
    bool enable_stereo_separation = false;      // Pseudo-stereo effect
    float stereo_separation_amount = 0.3f;      // Amount of stereo separation
    bool enable_reverb = false;                 // Basic reverb effect
    float reverb_amount = 0.1f;                // Reverb level

    // Quality settings
    enum class quality_preset {
        PERFORMANCE,    // Optimized for speed
        BALANCED,       // Good quality/performance balance
        QUALITY,        // Best quality, higher CPU usage
        AUTHENTIC       // Hardware-accurate, may be slower
    };
    quality_preset quality = quality_preset::BALANCED;
};

/**
 * Per-Channel NES Configuration
 */
struct nes_channel_config {
    bool enabled = true;
    float volume = 1.0f;
    float pan = 0.0f;           // -1.0 (left) to 1.0 (right)
    bool muted = false;

    // Channel-specific settings
    struct pulse_config {
        uint8_t default_duty_cycle = 2;        // 0-3 (12.5%, 25%, 50%, 75%)
        bool enable_sweep = true;              // Enable frequency sweep unit
        bool enable_envelope = true;           // Enable volume envelope
        uint8_t default_volume = 15;           // 0-15
    };

    struct triangle_config {
        bool enable_linear_counter = true;     // Enable linear counter
        uint8_t default_linear_counter = 127;  // Linear counter load value
        bool enable_length_counter = true;     // Enable length counter
    };

    struct noise_config {
        bool short_mode = false;               // Short vs long noise sequence
        uint8_t default_period = 8;           // Noise period (0-15)
        bool enable_envelope = true;           // Enable volume envelope
    };

    struct dmc_config {
        bool enable_irq = false;               // DMC IRQ generation
        uint8_t default_rate = 8;              // Sample rate index (0-15)
        bool enable_looping = false;           // Loop DMC samples
        uint16_t default_load_counter = 0;     // Default sample address
    };

    // Union for channel-specific settings
    enum class channel_type { PULSE1, PULSE2, TRIANGLE, NOISE, DMC };
    channel_type type;

    union specific_config {
        pulse_config pulse;
        triangle_config triangle;
        noise_config noise;
        dmc_config dmc;

        specific_config() {}
        ~specific_config() {}
    } specific;

    nes_channel_config(channel_type t) : type(t) {
        switch (type) {
            case channel_type::PULSE1:
            case channel_type::PULSE2:
                new(&specific.pulse) pulse_config{};
                break;
            case channel_type::TRIANGLE:
                new(&specific.triangle) triangle_config{};
                break;
            case channel_type::NOISE:
                new(&specific.noise) noise_config{};
                break;
            case channel_type::DMC:
                new(&specific.dmc) dmc_config{};
                break;
        }
    }
};

/**
 * NES File Processing Configuration
 */
struct nes_file_config {
    // File format support
    bool enable_midi_import = true;
    bool enable_musicxml_import = true;
    bool enable_nsf_import = false;             // NSF format (future)
    bool enable_pattern_export = true;

    // MIDI conversion settings
    bool auto_optimize_for_nes = true;          // Automatically optimize MIDI for NES
    bool preserve_original_timing = true;       // Keep original note timing
    bool quantize_to_nes_notes = true;         // Snap to NES-compatible notes
    uint8_t max_polyphony_per_channel = 1;     // NES channels are monophonic

    // Channel mapping preferences
    std::map<uint8_t, uint8_t> midi_to_nes_channel_map = {
        {0, 0},  // MIDI channel 0 -> NES Pulse 1
        {1, 1},  // MIDI channel 1 -> NES Pulse 2
        {2, 2},  // MIDI channel 2 -> NES Triangle
        {9, 3},  // MIDI channel 9 (drums) -> NES Noise
        {3, 4}   // MIDI channel 3 -> NES DMC
    };

    // Note range limits (MIDI note numbers)
    uint8_t min_pulse_note = 21;               // A0
    uint8_t max_pulse_note = 108;              // C8
    uint8_t min_triangle_note = 12;            // C0
    uint8_t max_triangle_note = 84;            // C6

    // File validation
    size_t max_file_size_mb = 100;
    bool strict_nes_compatibility = false;     // Reject non-NES compatible files
    bool create_backup_files = false;          // Backup original files
};

/**
 * Performance and Optimization Configuration
 */
struct nes_performance_config {
    // Threading options
    bool enable_multithreading = true;
    uint32_t audio_thread_priority = 95;       // Real-time priority (0-99)
    uint32_t worker_thread_count = 0;          // 0 = auto-detect

    // Buffer management
    uint32_t audio_buffer_count = 3;           // Triple buffering
    uint32_t max_audio_latency_ms = 50;        // Maximum acceptable latency
    uint32_t lookahead_buffer_ms = 20;         // Event scheduling lookahead

    // Memory optimization
    bool enable_sample_caching = true;         // Cache generated samples
    size_t sample_cache_size_mb = 16;          // Sample cache size
    bool enable_lazy_loading = true;           // Load resources on demand

    // CPU optimization
    bool enable_simd_optimization = true;      // Use SIMD instructions
    bool enable_fast_math = false;             // Less accurate but faster math
    uint32_t max_polyphony = 5;                // Maximum simultaneous notes (NES = 5)

    // Monitoring
    bool enable_performance_monitoring = true;
    bool enable_real_time_metrics = false;    // Real-time performance display
    uint32_t metrics_update_interval_ms = 100; // Metrics update frequency
};

/**
 * User Interface Configuration
 */
struct nes_ui_config {
    // Display preferences
    bool show_channel_meters = true;           // Audio level meters
    bool show_note_visualization = true;       // Real-time note display
    bool show_performance_metrics = false;     // Performance counters
    bool use_nes_color_scheme = true;          // NES-inspired colors

    // CLI behavior
    bool verbose_output = false;
    bool use_progress_bars = true;
    bool enable_colored_output = true;
    std::string log_level = "info";            // debug, info, warn, error

    // Help and documentation
    bool show_tips_on_startup = true;
    bool enable_command_suggestions = true;    // Suggest similar commands
    uint32_t command_history_size = 100;       // Command history buffer
};

/**
 * Complete NES Configuration - combines all configuration aspects
 */
struct nes_configuration {
    std::string config_version = "1.0";
    std::string config_name = "default";

    nes_hardware_config hardware;
    nes_audio_config audio;
    std::vector<nes_channel_config> channels;  // One for each NES channel
    nes_file_config file_processing;
    nes_performance_config performance;
    nes_ui_config ui;

    // Metadata
    std::string created_by = "NES Synthesizer";
    std::string description = "Default NES configuration";
    uint64_t created_timestamp = 0;           // Unix timestamp
    uint64_t modified_timestamp = 0;          // Unix timestamp

    // Initialize with default NES channel configurations
    nes_configuration() {
        channels.reserve(5);
        channels.emplace_back(nes_channel_config::channel_type::PULSE1);
        channels.emplace_back(nes_channel_config::channel_type::PULSE2);
        channels.emplace_back(nes_channel_config::channel_type::TRIANGLE);
        channels.emplace_back(nes_channel_config::channel_type::NOISE);
        channels.emplace_back(nes_channel_config::channel_type::DMC);
    }
};

/**
 * Configuration Validation and Management
 */
class nes_config_manager {
public:
    // Configuration loading and saving
    static bool load_configuration(const std::string& filename, nes_configuration& config);
    static bool save_configuration(const std::string& filename, const nes_configuration& config);

    // Validation
    static bool validate_configuration(const nes_configuration& config, std::string& error_message);
    static void apply_safe_defaults(nes_configuration& config);

    // Preset configurations
    static nes_configuration create_performance_preset();    // Optimized for speed
    static nes_configuration create_quality_preset();       // Optimized for audio quality
    static nes_configuration create_authentic_preset();     // Hardware-accurate
    static nes_configuration create_creative_preset();      // Enhanced features for music creation

    // Configuration conversion
    template<typename EngineConfig>
    static void apply_to_engine_config(const nes_configuration& nes_config, EngineConfig& engine_config);
    template<typename CliConfig>
    static void apply_to_cli_config(const nes_configuration& nes_config, CliConfig& cli_config);

    // Runtime configuration updates
    static bool update_audio_settings(nes_configuration& config, const nes_audio_config& new_audio);
    static bool update_channel_settings(nes_configuration& config, uint8_t channel,
                                       const nes_channel_config& new_settings);

    // Configuration discovery
    static std::vector<std::string> find_configuration_files(const std::string& directory = ".");
    static std::string get_default_config_path();
    static std::string get_user_config_directory();

private:
    static bool validate_audio_config(const nes_audio_config& config, std::string& error);
    static bool validate_channel_config(const nes_channel_config& config, std::string& error);
    static bool validate_file_config(const nes_file_config& config, std::string& error);
    static bool validate_performance_config(const nes_performance_config& config, std::string& error);
};

/**
 * Configuration File Format Support
 */
class nes_config_serializer {
public:
    // JSON format support
    static bool serialize_to_json(const nes_configuration& config, std::string& json_output);
    static bool deserialize_from_json(const std::string& json_input, nes_configuration& config);

    // INI format support (for simple configurations)
    static bool serialize_to_ini(const nes_configuration& config, std::string& ini_output);
    static bool deserialize_from_ini(const std::string& ini_input, nes_configuration& config);

    // Binary format (for performance)
    static bool serialize_to_binary(const nes_configuration& config, std::vector<uint8_t>& binary_output);
    static bool deserialize_from_binary(const std::vector<uint8_t>& binary_input, nes_configuration& config);

    // Configuration diff and merge
    static std::string generate_config_diff(const nes_configuration& old_config,
                                           const nes_configuration& new_config);
    static bool merge_configurations(const nes_configuration& base_config,
                                   const nes_configuration& overlay_config,
                                   nes_configuration& result_config);
};

} // namespace nes_config