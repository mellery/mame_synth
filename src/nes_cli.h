#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <iostream>
#include "nes_playback_engine.h"
#include "comprehensive_file_support.h"
#include "nes_config.h"
#include "nes_realtime_control.h"
#include "nes_channel_assignment.h"

/**
 * NES Synthesizer Command Line Interface
 *
 * Provides a comprehensive CLI for the NES synthesizer with commands for:
 * - File operations (load, validate, convert, analyze)
 * - Playback control (play, pause, stop, seek)
 * - Configuration (audio settings, NES parameters)
 * - Information display (metadata, formats, help)
 * - Batch operations (directory processing)
 */

class nes_cli {
public:
    // Command result structure
    struct command_result {
        bool success = false;
        std::string message;
        int exit_code = 0;
    };

    // CLI configuration
    struct cli_config {
        // Audio settings
        uint32_t sample_rate = 44100;
        uint32_t buffer_size = 1024;
        std::string audio_backend = "auto";  // auto, alsa, directsound, file
        std::string output_file = "output.wav";

        // Playback settings
        float master_volume = 1.0f;
        double tempo_scale = 1.0;
        bool enable_looping = false;
        uint32_t default_tempo_bpm = 120;

        // NES-specific settings
        bool enable_nonlinear_mixing = true;
        bool enable_highpass_filter = true;
        bool enable_lowpass_filter = true;
        float pulse_volume_scale = 1.0f;
        float triangle_volume_scale = 0.9f;
        float noise_volume_scale = 0.7f;
        float dmc_volume_scale = 0.5f;

        // Output settings
        bool verbose = false;
        bool quiet = false;
        bool show_progress = true;
        std::string log_level = "info";  // debug, info, warn, error

        // File processing
        bool enable_nes_optimization = true;
        bool backup_on_conversion = false;
        size_t max_file_size_mb = 100;
    };

    // Command function type
    using command_func_t = std::function<command_result(const std::vector<std::string>&)>;

    nes_cli();
    explicit nes_cli(const cli_config& config);
    ~nes_cli();

    // Main entry points
    command_result run(int argc, char* argv[]);
    command_result run_command(const std::string& cmd_line);
    command_result run_interactive();
    command_result run_commands_from_stdin();

    // Configuration
    void set_config(const cli_config& config);
    cli_config get_config() const { return m_config; }
    void load_config_file(const std::string& filename);
    void save_config_file(const std::string& filename) const;

    // Command registration (for extensibility)
    void register_command(const std::string& name, command_func_t func, const std::string& description);

    // Initialization (can be called explicitly for testing)
    bool initialize();
    void shutdown();

private:
    cli_config m_config;
    nes_config::nes_configuration m_nes_config;
    std::unique_ptr<nes_playback_engine> m_engine;
    std::unique_ptr<comprehensive_file_manager> m_file_manager;
    std::unique_ptr<nes_realtime::realtime_parameter_controller> m_realtime_controller;
    std::unique_ptr<nes_realtime::nes_realtime_control_interface> m_realtime_interface;
    std::unique_ptr<nes_realtime::midi_realtime_handler> m_midi_handler;
    std::unique_ptr<nes_channel_assignment::channel_assignment_engine> m_assignment_engine;
    std::map<std::string, command_func_t> m_commands;
    std::map<std::string, std::string> m_command_descriptions;
    bool m_initialized = false;

    // Internal methods
    void register_builtin_commands();
    std::vector<std::string> parse_command_line(const std::string& cmd_line);
    void print_usage() const;
    void print_version() const;
    void print_help(const std::string& command = "") const;

    // Logging
    void log_debug(const std::string& message) const;
    void log_info(const std::string& message) const;
    void log_warn(const std::string& message) const;
    void log_error(const std::string& message) const;

    // Built-in command implementations
    command_result cmd_help(const std::vector<std::string>& args);
    command_result cmd_version(const std::vector<std::string>& args);
    command_result cmd_config(const std::vector<std::string>& args);

    // File commands
    command_result cmd_load(const std::vector<std::string>& args);
    command_result cmd_validate(const std::vector<std::string>& args);
    command_result cmd_info(const std::vector<std::string>& args);
    command_result cmd_analyze(const std::vector<std::string>& args);
    command_result cmd_convert(const std::vector<std::string>& args);
    command_result cmd_export(const std::vector<std::string>& args);

    // Playback commands
    command_result cmd_play(const std::vector<std::string>& args);
    command_result cmd_pause(const std::vector<std::string>& args);
    command_result cmd_stop(const std::vector<std::string>& args);
    command_result cmd_resume(const std::vector<std::string>& args);
    command_result cmd_seek(const std::vector<std::string>& args);
    command_result cmd_status(const std::vector<std::string>& args);

    // Audio control commands
    command_result cmd_volume(const std::vector<std::string>& args);
    command_result cmd_tempo(const std::vector<std::string>& args);
    command_result cmd_loop(const std::vector<std::string>& args);
    command_result cmd_channels(const std::vector<std::string>& args);

    // NES-specific commands
    command_result cmd_nes_settings(const std::vector<std::string>& args);
    command_result cmd_nes_channels(const std::vector<std::string>& args);
    command_result cmd_nes_optimize(const std::vector<std::string>& args);

    // Batch commands
    command_result cmd_batch_validate(const std::vector<std::string>& args);
    command_result cmd_batch_convert(const std::vector<std::string>& args);
    command_result cmd_batch_analyze(const std::vector<std::string>& args);

    // Information commands
    command_result cmd_formats(const std::vector<std::string>& args);
    command_result cmd_backends(const std::vector<std::string>& args);
    command_result cmd_metrics(const std::vector<std::string>& args);

    // NES Configuration commands
    command_result cmd_config_load(const std::vector<std::string>& args);
    command_result cmd_config_save(const std::vector<std::string>& args);
    command_result cmd_config_show(const std::vector<std::string>& args);
    command_result cmd_config_reset(const std::vector<std::string>& args);
    command_result cmd_config_preset(const std::vector<std::string>& args);
    command_result cmd_config_validate(const std::vector<std::string>& args);
    command_result cmd_config_list(const std::vector<std::string>& args);
    command_result cmd_config_set(const std::vector<std::string>& args);
    command_result cmd_config_get(const std::vector<std::string>& args);

    // Real-time control commands
    command_result cmd_realtime_set(const std::vector<std::string>& args);
    command_result cmd_realtime_get(const std::vector<std::string>& args);
    command_result cmd_realtime_preset(const std::vector<std::string>& args);
    command_result cmd_realtime_stats(const std::vector<std::string>& args);
    command_result cmd_realtime_midi_map(const std::vector<std::string>& args);
    command_result cmd_realtime_midi_learn(const std::vector<std::string>& args);
    command_result cmd_realtime_automation(const std::vector<std::string>& args);
    command_result cmd_channel_volume(const std::vector<std::string>& args);
    command_result cmd_channel_pan(const std::vector<std::string>& args);
    command_result cmd_channel_mute(const std::vector<std::string>& args);
    command_result cmd_pulse_duty(const std::vector<std::string>& args);
    command_result cmd_filter_cutoff(const std::vector<std::string>& args);
    command_result cmd_master_control(const std::vector<std::string>& args);

    // Channel assignment commands
    command_result cmd_analyze_tracks(const std::vector<std::string>& args);
    command_result cmd_assignment_strategy(const std::vector<std::string>& args);
    command_result cmd_assign_channel(const std::vector<std::string>& args);
    command_result cmd_auto_assign(const std::vector<std::string>& args);
    command_result cmd_show_assignment(const std::vector<std::string>& args);
    command_result cmd_assignment_report(const std::vector<std::string>& args);

    // Utility methods
    bool validate_file_path(const std::string& path, bool must_exist = true) const;
    std::string format_duration(double seconds) const;
    std::string format_size(size_t bytes) const;
    void print_file_info(const enhanced_music_metadata& metadata) const;
    void print_progress_bar(double percentage, int width = 50) const;
    audio_stream_factory::backend_type parse_audio_backend(const std::string& backend) const;

    // Configuration management utilities
    void sync_config_to_engine();
    void sync_config_from_nes_config();
    void print_config_summary() const;
    void print_config_section(const std::string& section) const;
};

/**
 * CLI Utilities - Helper functions for command-line operations
 */
namespace nes_cli_utils {
    // Argument parsing
    std::map<std::string, std::string> parse_key_value_args(const std::vector<std::string>& args);
    bool parse_bool_arg(const std::string& value);
    double parse_time_arg(const std::string& value); // Supports formats like "1:30", "90s", "1.5m"

    // File operations
    std::vector<std::string> find_music_files(const std::string& directory, bool recursive = false);
    std::string make_backup_filename(const std::string& original);
    bool is_music_file(const std::string& filename);

    // Display formatting
    std::string format_table(const std::vector<std::vector<std::string>>& data,
                           const std::vector<std::string>& headers = {});
    std::string colorize_text(const std::string& text, const std::string& color);
    std::string format_json(const enhanced_music_metadata& metadata);

    // Interactive helpers
    std::string prompt_user(const std::string& question, const std::string& default_value = "");
    bool confirm_user(const std::string& question, bool default_value = false);
    std::string select_from_list(const std::vector<std::string>& options, const std::string& prompt);
}

/**
 * CLI Application Entry Point
 */
class nes_cli_app {
public:
    static int main(int argc, char* argv[]);
    static void setup_signal_handlers();
    static void cleanup();

private:
    static std::unique_ptr<nes_cli> s_cli;
    static void signal_handler(int signal);
    static void print_startup_banner();
    static void print_shutdown_message();
};