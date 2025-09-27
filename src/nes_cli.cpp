#include "nes_cli.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <thread>
#include <chrono>
#include <signal.h>
#include <filesystem>
#include <regex>
#include <unistd.h>

// CLI Implementation
nes_cli::nes_cli() : m_config({}) {
    log_info("NES Synthesizer CLI initializing with default config...");
}

nes_cli::nes_cli(const cli_config& config) : m_config(config) {
    log_info("NES Synthesizer CLI initializing...");
}

nes_cli::~nes_cli() {
    shutdown();
}

nes_cli::command_result nes_cli::run(int argc, char* argv[]) {
    if (!initialize()) {
        return {false, "Failed to initialize NES CLI", 1};
    }

    // Parse command line arguments
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (args.empty()) {
        // If no arguments and stdin is not a terminal (i.e. piped input), read commands from stdin
        if (!isatty(STDIN_FILENO)) {
            return run_commands_from_stdin();
        } else {
            print_usage();
            return {true, "Usage displayed", 0};
        }
    }

    // Handle global options first
    auto it = args.begin();
    while (it != args.end()) {
        if (*it == "--help" || *it == "-h") {
            print_help();
            return {true, "Help displayed", 0};
        } else if (*it == "--version" || *it == "-v") {
            print_version();
            return {true, "Version displayed", 0};
        } else if (*it == "--verbose") {
            m_config.verbose = true;
            it = args.erase(it);
        } else if (*it == "--quiet" || *it == "-q") {
            m_config.quiet = true;
            it = args.erase(it);
        } else if (*it == "--interactive" || *it == "-i") {
            args.erase(it);
            return run_interactive();
        } else {
            ++it;
        }
    }

    if (args.empty()) {
        // If no arguments and stdin is not a terminal (i.e. piped input), read commands from stdin
        if (!isatty(STDIN_FILENO)) {
            std::cerr << "[DEBUG] Detected piped input, processing commands from stdin" << std::endl;
            return run_commands_from_stdin();
        } else {
            print_usage();
            return {true, "Usage displayed", 0};
        }
    }

    // Execute single command
    std::string command = args[0];
    args.erase(args.begin());

    auto cmd_it = m_commands.find(command);
    if (cmd_it == m_commands.end()) {
        return {false, "Unknown command: " + command + ". Use 'help' for available commands.", 1};
    }

    try {
        return cmd_it->second(args);
    } catch (const std::exception& e) {
        return {false, "Command failed: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::run_command(const std::string& cmd_line) {
    auto args = parse_command_line(cmd_line);
    if (args.empty()) {
        return {true, "", 0};
    }

    std::string command = args[0];
    args.erase(args.begin());

    auto cmd_it = m_commands.find(command);
    if (cmd_it == m_commands.end()) {
        return {false, "Unknown command: " + command, 1};
    }

    try {
        return cmd_it->second(args);
    } catch (const std::exception& e) {
        return {false, "Command failed: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::run_interactive() {
    if (!initialize()) {
        return {false, "Failed to initialize NES CLI", 1};
    }

    std::cout << "\n🎵 NES Synthesizer Interactive Mode 🎵\n";
    std::cout << "Type 'help' for commands, 'quit' to exit.\n\n";

    std::string line;
    while (true) {
        std::cout << "nes> ";
        std::cout.flush(); // Ensure prompt is displayed

        if (!std::getline(std::cin, line)) {
            // Input stream ended (EOF or error)
            std::cout << "\nInput stream ended" << std::endl;
            break;
        }

        if (line == "quit" || line == "exit" || line == "q") {
            break;
        }

        if (line.empty()) {
            continue;
        }

        auto result = run_command(line);
        if (!result.success && !result.message.empty()) {
            log_error(result.message);
        } else if (!result.message.empty()) {
            std::cout << result.message << std::endl;
        }
    }

    std::cout << "Goodbye!\n";
    return {true, "Interactive session completed", 0};
}

nes_cli::command_result nes_cli::run_commands_from_stdin() {
    if (!initialize()) {
        return {false, "Failed to initialize NES CLI", 1};
    }

    std::string line;
    int command_count = 0;
    bool has_errors = false;

    while (std::getline(std::cin, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        command_count++;
        auto result = run_command(line);

        if (!result.success) {
            log_error("Command failed: " + result.message);
            has_errors = true;
        } else if (!result.message.empty()) {
            std::cout << result.message << std::endl;
        }
    }

    if (command_count == 0) {
        return {true, "No commands processed", 0};
    }

    return {!has_errors,
            "Processed " + std::to_string(command_count) + " commands" +
            (has_errors ? " (with errors)" : ""),
            has_errors ? 1 : 0};
}

bool nes_cli::initialize() {
    if (m_initialized) {
        return true;
    }

    try {
        // Create NES playback engine configuration
        nes_playback_engine::engine_config engine_config;
        engine_config.sample_rate = m_config.sample_rate;
        engine_config.buffer_size = m_config.buffer_size;
        engine_config.audio_backend = parse_audio_backend(m_config.audio_backend);
        engine_config.enable_performance_monitoring = true;
        engine_config.enable_midi_support = true;
        engine_config.enable_musicxml_support = true;
        engine_config.enable_pattern_support = true;

        // Configure NES mixer
        engine_config.mixer_config.enable_nonlinear_mixing = m_config.enable_nonlinear_mixing;
        engine_config.mixer_config.enable_highpass_filter = m_config.enable_highpass_filter;
        engine_config.mixer_config.enable_lowpass_filter = m_config.enable_lowpass_filter;
        engine_config.mixer_config.pulse_volume_scale = m_config.pulse_volume_scale;
        engine_config.mixer_config.triangle_volume_scale = m_config.triangle_volume_scale;
        engine_config.mixer_config.noise_volume_scale = m_config.noise_volume_scale;
        engine_config.mixer_config.dmc_volume_scale = m_config.dmc_volume_scale;

        // Create engine
        m_engine = std::make_unique<nes_playback_engine>(engine_config);
        if (!m_engine->initialize()) {
            log_error("Failed to initialize NES playback engine");
            return false;
        }

        // Create file manager
        m_file_manager = comprehensive_file_support_factory::create_nes_optimized_manager();
        if (!m_file_manager) {
            log_error("Failed to create file manager");
            return false;
        }

        // Create real-time control system
        m_realtime_controller = std::make_unique<nes_realtime::realtime_parameter_controller>(
            nes_realtime::realtime_parameter_controller::control_mode::SMOOTH);

        // Integrate real-time controller with engine
        m_realtime_controller->set_playback_engine(m_engine.get());
        // TODO: Add get_mixer() and get_sequencer() methods to nes_playback_engine
        // for better integration

        // Create high-level interfaces
        m_realtime_interface = std::make_unique<nes_realtime::nes_realtime_control_interface>(*m_realtime_controller);
        m_midi_handler = std::make_unique<nes_realtime::midi_realtime_handler>(*m_realtime_controller);

        // Start real-time processing
        m_realtime_controller->start_processing();

        // Register built-in commands
        register_builtin_commands();

        m_initialized = true;
        log_info("NES CLI initialized successfully");
        return true;

    } catch (const std::exception& e) {
        log_error("Initialization failed: " + std::string(e.what()));
        return false;
    }
}

void nes_cli::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Shutdown real-time control system
    if (m_realtime_controller) {
        m_realtime_controller->stop_processing();
        m_realtime_controller.reset();
    }
    m_realtime_interface.reset();
    m_midi_handler.reset();

    if (m_engine) {
        m_engine->shutdown();
        m_engine.reset();
    }

    m_file_manager.reset();
    m_initialized = false;
    log_info("NES CLI shutdown complete");
}

void nes_cli::register_builtin_commands() {
    // Help and info commands
    register_command("help", [this](const auto& args) { return cmd_help(args); },
                    "Show help information");
    register_command("version", [this](const auto& args) { return cmd_version(args); },
                    "Show version information");
    register_command("config", [this](const auto& args) { return cmd_config(args); },
                    "Show or modify configuration");

    // File commands
    register_command("load", [this](const auto& args) { return cmd_load(args); },
                    "Load a music file");
    register_command("validate", [this](const auto& args) { return cmd_validate(args); },
                    "Validate a music file");
    register_command("info", [this](const auto& args) { return cmd_info(args); },
                    "Show file information");
    register_command("analyze", [this](const auto& args) { return cmd_analyze(args); },
                    "Analyze file for NES compatibility");
    register_command("convert", [this](const auto& args) { return cmd_convert(args); },
                    "Convert between file formats");
    register_command("export", [this](const auto& args) { return cmd_export(args); },
                    "Export audio to WAV file");

    // Playback commands
    register_command("play", [this](const auto& args) { return cmd_play(args); },
                    "Start playback");
    register_command("pause", [this](const auto& args) { return cmd_pause(args); },
                    "Pause playback");
    register_command("stop", [this](const auto& args) { return cmd_stop(args); },
                    "Stop playback");
    register_command("resume", [this](const auto& args) { return cmd_resume(args); },
                    "Resume playback");
    register_command("seek", [this](const auto& args) { return cmd_seek(args); },
                    "Seek to position");
    register_command("status", [this](const auto& args) { return cmd_status(args); },
                    "Show playback status");

    // Audio control commands
    register_command("volume", [this](const auto& args) { return cmd_volume(args); },
                    "Control master volume");
    register_command("tempo", [this](const auto& args) { return cmd_tempo(args); },
                    "Control playback tempo");
    register_command("loop", [this](const auto& args) { return cmd_loop(args); },
                    "Enable/disable looping");
    register_command("channels", [this](const auto& args) { return cmd_channels(args); },
                    "Control channel settings");

    // NES-specific commands
    register_command("nes-settings", [this](const auto& args) { return cmd_nes_settings(args); },
                    "Configure NES APU settings");
    register_command("nes-channels", [this](const auto& args) { return cmd_nes_channels(args); },
                    "Control NES channels");
    register_command("nes-optimize", [this](const auto& args) { return cmd_nes_optimize(args); },
                    "Optimize file for NES playback");

    // Batch commands
    register_command("batch-validate", [this](const auto& args) { return cmd_batch_validate(args); },
                    "Validate multiple files");
    register_command("batch-convert", [this](const auto& args) { return cmd_batch_convert(args); },
                    "Convert multiple files");
    register_command("batch-analyze", [this](const auto& args) { return cmd_batch_analyze(args); },
                    "Analyze multiple files");

    // Information commands
    register_command("formats", [this](const auto& args) { return cmd_formats(args); },
                    "List supported file formats");
    register_command("backends", [this](const auto& args) { return cmd_backends(args); },
                    "List available audio backends");
    register_command("metrics", [this](const auto& args) { return cmd_metrics(args); },
                    "Show performance metrics");

    // NES Configuration commands
    register_command("config-load", [this](const auto& args) { return cmd_config_load(args); },
                    "Load configuration from file");
    register_command("config-save", [this](const auto& args) { return cmd_config_save(args); },
                    "Save configuration to file");
    register_command("config-show", [this](const auto& args) { return cmd_config_show(args); },
                    "Show current configuration");
    register_command("config-reset", [this](const auto& args) { return cmd_config_reset(args); },
                    "Reset configuration to defaults");
    register_command("config-preset", [this](const auto& args) { return cmd_config_preset(args); },
                    "Load configuration preset (performance, quality, authentic, creative)");
    register_command("config-validate", [this](const auto& args) { return cmd_config_validate(args); },
                    "Validate configuration file");
    register_command("config-list", [this](const auto& args) { return cmd_config_list(args); },
                    "List available configuration files");
    register_command("config-set", [this](const auto& args) { return cmd_config_set(args); },
                    "Set configuration value (config-set section.key value)");
    register_command("config-get", [this](const auto& args) { return cmd_config_get(args); },
                    "Get configuration value (config-get section.key)");

    // Real-time control commands
    register_command("realtime-set", [this](const auto& args) { return cmd_realtime_set(args); },
                    "Set real-time parameter (realtime-set PARAM [channel] value)");
    register_command("realtime-get", [this](const auto& args) { return cmd_realtime_get(args); },
                    "Get real-time parameter value");
    register_command("realtime-preset", [this](const auto& args) { return cmd_realtime_preset(args); },
                    "Load/save real-time parameter preset");
    register_command("realtime-stats", [this](const auto& args) { return cmd_realtime_stats(args); },
                    "Show real-time control statistics");
    register_command("midi-map", [this](const auto& args) { return cmd_realtime_midi_map(args); },
                    "Map MIDI controller to parameter");
    register_command("midi-learn", [this](const auto& args) { return cmd_realtime_midi_learn(args); },
                    "Enter MIDI learn mode for parameter");
    register_command("automation", [this](const auto& args) { return cmd_realtime_automation(args); },
                    "Control parameter automation");

    // Convenience real-time commands
    register_command("cv", [this](const auto& args) { return cmd_channel_volume(args); },
                    "Set channel volume (cv <channel> <volume>)");
    register_command("cp", [this](const auto& args) { return cmd_channel_pan(args); },
                    "Set channel pan (cp <channel> <pan>)");
    register_command("cm", [this](const auto& args) { return cmd_channel_mute(args); },
                    "Mute/unmute channel (cm <channel> [on|off])");
    register_command("pd", [this](const auto& args) { return cmd_pulse_duty(args); },
                    "Set pulse duty cycle (pd <pulse_channel> <duty>)");
    register_command("fc", [this](const auto& args) { return cmd_filter_cutoff(args); },
                    "Set filter cutoff (fc <type> <frequency>)");
    register_command("mc", [this](const auto& args) { return cmd_master_control(args); },
                    "Master controls (mc volume|tempo|pitch <value>)");
}

void nes_cli::register_command(const std::string& name, command_func_t func, const std::string& description) {
    m_commands[name] = func;
    m_command_descriptions[name] = description;
}

std::vector<std::string> nes_cli::parse_command_line(const std::string& cmd_line) {
    std::vector<std::string> args;
    std::istringstream iss(cmd_line);
    std::string token;

    bool in_quotes = false;
    std::string current_arg;

    for (char c : cmd_line) {
        if (c == '"' && !in_quotes) {
            in_quotes = true;
        } else if (c == '"' && in_quotes) {
            in_quotes = false;
        } else if (c == ' ' && !in_quotes) {
            if (!current_arg.empty()) {
                args.push_back(current_arg);
                current_arg.clear();
            }
        } else {
            current_arg += c;
        }
    }

    if (!current_arg.empty()) {
        args.push_back(current_arg);
    }

    return args;
}

void nes_cli::print_usage() const {
    std::cout << "NES Synthesizer - Command Line Interface\n\n";
    std::cout << "Usage: mame_synth [options] <command> [arguments]\n\n";
    std::cout << "Global Options:\n";
    std::cout << "  -h, --help        Show this help message\n";
    std::cout << "  -v, --version     Show version information\n";
    std::cout << "  -i, --interactive Enter interactive mode\n";
    std::cout << "  -q, --quiet       Suppress output messages\n";
    std::cout << "  --verbose         Show detailed output\n\n";
    std::cout << "Common Commands:\n";
    std::cout << "  load <file>       Load and play a music file\n";
    std::cout << "  validate <file>   Validate a music file\n";
    std::cout << "  info <file>       Show file information\n";
    std::cout << "  play              Start playback\n";
    std::cout << "  stop              Stop playback\n";
    std::cout << "  help              Show all available commands\n\n";
    std::cout << "Examples:\n";
    std::cout << "  mame_synth load song.mid\n";
    std::cout << "  mame_synth validate *.mid\n";
    std::cout << "  mame_synth --interactive\n";
}

void nes_cli::print_version() const {
    std::cout << "NES Synthesizer CLI v1.0.0\n";
    std::cout << "Built with MAME audio core\n";
    std::cout << "Phase 5: User Interface & Polish\n";
}

void nes_cli::print_help(const std::string& command) const {
    if (command.empty()) {
        std::cout << "Available Commands:\n\n";

        // Group commands by category
        std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> categories = {
            {"File Operations", {}},
            {"Playback Control", {}},
            {"Audio Control", {}},
            {"NES Specific", {}},
            {"Batch Operations", {}},
            {"Information", {}},
            {"System", {}}
        };

        // Categorize commands
        for (const auto& [cmd, desc] : m_command_descriptions) {
            if (cmd.find("load") == 0 || cmd.find("validate") == 0 || cmd.find("info") == 0 ||
                cmd.find("analyze") == 0 || cmd.find("convert") == 0 || cmd.find("export") == 0) {
                categories[0].second.emplace_back(cmd, desc);
            } else if (cmd.find("play") == 0 || cmd.find("pause") == 0 || cmd.find("stop") == 0 ||
                      cmd.find("resume") == 0 || cmd.find("seek") == 0 || cmd.find("status") == 0) {
                categories[1].second.emplace_back(cmd, desc);
            } else if (cmd.find("volume") == 0 || cmd.find("tempo") == 0 || cmd.find("loop") == 0 ||
                      cmd.find("channels") == 0) {
                categories[2].second.emplace_back(cmd, desc);
            } else if (cmd.find("nes-") == 0) {
                categories[3].second.emplace_back(cmd, desc);
            } else if (cmd.find("batch-") == 0) {
                categories[4].second.emplace_back(cmd, desc);
            } else if (cmd.find("formats") == 0 || cmd.find("backends") == 0 || cmd.find("metrics") == 0) {
                categories[5].second.emplace_back(cmd, desc);
            } else {
                categories[6].second.emplace_back(cmd, desc);
            }
        }

        for (const auto& [category, commands] : categories) {
            if (!commands.empty()) {
                std::cout << category << ":\n";
                for (const auto& [cmd, desc] : commands) {
                    std::cout << "  " << std::left << std::setw(20) << cmd << desc << "\n";
                }
                std::cout << "\n";
            }
        }
    } else {
        auto it = m_command_descriptions.find(command);
        if (it != m_command_descriptions.end()) {
            std::cout << "Command: " << command << "\n";
            std::cout << "Description: " << it->second << "\n";
        } else {
            std::cout << "Unknown command: " << command << "\n";
        }
    }
}

// Logging methods
void nes_cli::log_debug(const std::string& message) const {
    if (m_config.verbose && !m_config.quiet) {
        std::cout << "[DEBUG] " << message << std::endl;
    }
}

void nes_cli::log_info(const std::string& message) const {
    if (!m_config.quiet) {
        std::cout << "[INFO] " << message << std::endl;
    }
}

void nes_cli::log_warn(const std::string& message) const {
    if (!m_config.quiet) {
        std::cout << "[WARN] " << message << std::endl;
    }
}

void nes_cli::log_error(const std::string& message) const {
    std::cerr << "[ERROR] " << message << std::endl;
}

audio_stream_factory::backend_type nes_cli::parse_audio_backend(const std::string& backend) const {
    if (backend == "auto") return audio_stream_factory::backend_type::AUTO;
    if (backend == "alsa") return audio_stream_factory::backend_type::ALSA;
    if (backend == "directsound") return audio_stream_factory::backend_type::DIRECTSOUND;
    if (backend == "file") return audio_stream_factory::backend_type::FILE_OUTPUT;
    return audio_stream_factory::backend_type::AUTO;
}

// Utility methods
bool nes_cli::validate_file_path(const std::string& path, bool must_exist) const {
    if (must_exist) {
        return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
    }
    return !path.empty();
}

std::string nes_cli::format_duration(double seconds) const {
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    int centisecs = static_cast<int>((seconds - static_cast<int>(seconds)) * 100);

    std::ostringstream oss;
    oss << mins << ":" << std::setfill('0') << std::setw(2) << secs
        << "." << std::setw(2) << centisecs;
    return oss.str();
}

std::string nes_cli::format_size(size_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit < 3) {
        size /= 1024.0;
        unit++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit];
    return oss.str();
}

// Command implementations
nes_cli::command_result nes_cli::cmd_help(const std::vector<std::string>& args) {
    std::string command = args.empty() ? "" : args[0];
    print_help(command);
    return {true, "", 0};
}

nes_cli::command_result nes_cli::cmd_version(const std::vector<std::string>& args) {
    print_version();
    return {true, "", 0};
}

nes_cli::command_result nes_cli::cmd_config(const std::vector<std::string>& args) {
    if (args.empty()) {
        // Show current configuration
        std::cout << "Current Configuration:\n";
        std::cout << "  Sample Rate: " << m_config.sample_rate << " Hz\n";
        std::cout << "  Buffer Size: " << m_config.buffer_size << " frames\n";
        std::cout << "  Audio Backend: " << m_config.audio_backend << "\n";
        std::cout << "  Master Volume: " << (m_config.master_volume * 100) << "%\n";
        std::cout << "  Tempo Scale: " << m_config.tempo_scale << "x\n";
        std::cout << "  Loop Enabled: " << (m_config.enable_looping ? "yes" : "no") << "\n";
        std::cout << "  NES Mixing: " << (m_config.enable_nonlinear_mixing ? "hardware-accurate" : "linear") << "\n";
        std::cout << "  High-pass Filter: " << (m_config.enable_highpass_filter ? "enabled" : "disabled") << "\n";
        std::cout << "  Low-pass Filter: " << (m_config.enable_lowpass_filter ? "enabled" : "disabled") << "\n";
        return {true, "", 0};
    }

    // TODO: Implement configuration modification
    return {false, "Configuration modification not yet implemented", 1};
}

// File command implementations
nes_cli::command_result nes_cli::cmd_load(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {false, "Usage: load <filename>", 1};
    }

    std::string filename = args[0];
    if (!validate_file_path(filename)) {
        return {false, "File not found: " + filename, 1};
    }

    log_info("Loading file: " + filename);

    try {
        if (m_config.enable_nes_optimization) {
            if (!m_engine->load_with_nes_optimization(filename)) {
                return {false, "Failed to load file with NES optimization: " + filename, 1};
            }
        } else {
            if (!m_engine->load_file_enhanced(filename)) {
                return {false, "Failed to load file: " + filename, 1};
            }
        }

        auto info = m_engine->get_current_music_info();
        std::ostringstream oss;
        oss << "✓ Loaded: " << info.title;
        if (!info.artist.empty()) oss << " by " << info.artist;
        oss << " (" << format_duration(info.duration_seconds) << ")";

        return {true, oss.str(), 0};

    } catch (const std::exception& e) {
        return {false, "Error loading file: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::cmd_validate(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {false, "Usage: validate <filename>", 1};
    }

    std::string filename = args[0];
    if (!validate_file_path(filename)) {
        return {false, "File not found: " + filename, 1};
    }

    try {
        auto validation = m_engine->validate_file(filename);

        std::ostringstream oss;
        oss << "Validation: " << filename << "\n";
        oss << "  Format: " << validation.format_detected << "\n";
        oss << "  Valid: " << (validation.is_valid ? "✓ Yes" : "✗ No") << "\n";

        if (validation.has_errors()) {
            oss << "  Errors:\n";
            for (const auto& error : validation.errors) {
                oss << "    ✗ " << error << "\n";
            }
        }

        if (validation.has_warnings()) {
            oss << "  Warnings:\n";
            for (const auto& warning : validation.warnings) {
                oss << "    ⚠ " << warning << "\n";
            }
        }

        for (const auto& info : validation.info_messages) {
            oss << "    ℹ " << info << "\n";
        }

        return {true, oss.str(), validation.is_valid ? 0 : 1};

    } catch (const std::exception& e) {
        return {false, "Error validating file: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::cmd_info(const std::vector<std::string>& args) {
    if (args.empty()) {
        // Show info for currently loaded file
        if (m_engine->get_state() == nes_playback_engine::engine_state::UNINITIALIZED) {
            return {false, "No file loaded. Usage: info <filename>", 1};
        }

        auto info = m_engine->get_current_music_info();
        auto metadata = m_engine->get_current_metadata();

        std::ostringstream oss;
        oss << "Current File Information:\n";
        oss << "  File: " << info.filename << "\n";
        oss << "  Title: " << info.title << "\n";
        if (!info.artist.empty()) oss << "  Artist: " << info.artist << "\n";
        if (!info.album.empty()) oss << "  Album: " << info.album << "\n";
        oss << "  Format: " << info.format << "\n";
        oss << "  Duration: " << format_duration(info.duration_seconds) << "\n";
        oss << "  Notes: " << info.note_count << "\n";
        oss << "  Channels: " << info.channel_count << "\n";
        oss << "  Tempo: " << info.tempo_bpm << " BPM\n";

        if (!metadata.filename.empty()) {
            oss << "  NES Compatible: " << (metadata.nes_analysis.is_nes_compatible ? "✓ Yes" : "✗ No") << "\n";
            if (metadata.nes_analysis.is_nes_compatible) {
                oss << "  Channel Usage: P1=" << static_cast<int>(metadata.nes_analysis.pulse1_usage_percentage)
                    << "% P2=" << static_cast<int>(metadata.nes_analysis.pulse2_usage_percentage)
                    << "% T=" << static_cast<int>(metadata.nes_analysis.triangle_usage_percentage)
                    << "% N=" << static_cast<int>(metadata.nes_analysis.noise_usage_percentage) << "%\n";
            }
        }

        return {true, oss.str(), 0};
    }

    std::string filename = args[0];
    if (!validate_file_path(filename)) {
        return {false, "File not found: " + filename, 1};
    }

    try {
        enhanced_music_metadata metadata;
        music_data temp_data;

        if (!m_file_manager->load_file(filename, temp_data, metadata)) {
            return {false, "Failed to load file for analysis: " + filename, 1};
        }

        std::ostringstream oss;
        oss << "File Information: " << filename << "\n";
        oss << "  Title: " << metadata.title << "\n";
        if (!metadata.artist.empty()) oss << "  Artist: " << metadata.artist << "\n";
        if (!metadata.album.empty()) oss << "  Album: " << metadata.album << "\n";
        oss << "  Format: " << metadata.file_format << "\n";
        oss << "  Size: " << format_size(metadata.file_size_bytes) << "\n";
        oss << "  Duration: " << metadata.format_duration() << "\n";
        oss << "  Notes: " << metadata.total_notes << "\n";
        oss << "  Channels: " << metadata.unique_channels_used << "\n";
        oss << "  Tempo: " << metadata.default_tempo_bpm << " BPM\n";
        oss << "  Ticks per quarter: " << metadata.ticks_per_quarter << "\n";
        oss << "  NES Compatible: " << (metadata.nes_analysis.is_nes_compatible ? "✓ Yes" : "✗ No") << "\n";

        return {true, oss.str(), 0};

    } catch (const std::exception& e) {
        return {false, "Error analyzing file: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::cmd_analyze(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {false, "Usage: analyze <filename>", 1};
    }

    std::string filename = args[0];
    if (!validate_file_path(filename)) {
        return {false, "File not found: " + filename, 1};
    }

    try {
        enhanced_music_metadata metadata;
        music_data temp_data;

        if (!m_file_manager->load_file(filename, temp_data, metadata)) {
            return {false, "Failed to load file for analysis: " + filename, 1};
        }

        std::ostringstream oss;
        oss << "NES Compatibility Analysis: " << filename << "\n";
        oss << "  Compatible: " << (metadata.nes_analysis.is_nes_compatible ? "✓ Yes" : "✗ No") << "\n";

        if (metadata.nes_analysis.is_nes_compatible) {
            oss << "  Channel Usage:\n";
            oss << "    Pulse 1: " << static_cast<int>(metadata.nes_analysis.pulse1_usage_percentage) << "%\n";
            oss << "    Pulse 2: " << static_cast<int>(metadata.nes_analysis.pulse2_usage_percentage) << "%\n";
            oss << "    Triangle: " << static_cast<int>(metadata.nes_analysis.triangle_usage_percentage) << "%\n";
            oss << "    Noise: " << static_cast<int>(metadata.nes_analysis.noise_usage_percentage) << "%\n";
            oss << "    DMC: " << static_cast<int>(metadata.nes_analysis.dmc_usage_percentage) << "%\n";
        }

        if (!metadata.nes_analysis.compatibility_warnings.empty()) {
            oss << "  Warnings:\n";
            for (const auto& warning : metadata.nes_analysis.compatibility_warnings) {
                oss << "    ⚠ " << warning << "\n";
            }
        }

        if (!metadata.nes_analysis.optimization_suggestions.empty()) {
            oss << "  Suggestions:\n";
            for (const auto& suggestion : metadata.nes_analysis.optimization_suggestions) {
                oss << "    💡 " << suggestion << "\n";
            }
        }

        return {true, oss.str(), 0};

    } catch (const std::exception& e) {
        return {false, "Error analyzing file: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::cmd_convert(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        return {false, "Usage: convert <input_file> <output_file>", 1};
    }

    std::string input_file = args[0];
    std::string output_file = args[1];

    if (!validate_file_path(input_file)) {
        return {false, "Input file not found: " + input_file, 1};
    }

    try {
        if (m_file_manager->convert_file(input_file, output_file)) {
            return {true, "✓ Converted: " + input_file + " -> " + output_file, 0};
        } else {
            return {false, "Conversion failed: " + input_file + " -> " + output_file, 1};
        }

    } catch (const std::exception& e) {
        return {false, "Error converting file: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::cmd_export(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {false, "Usage: export <output_file.wav> [sample_rate]", 1};
    }

    std::string output_file = args[0];
    uint32_t sample_rate = args.size() > 1 ? std::stoi(args[1]) : 44100;

    if (m_engine->get_state() == nes_playback_engine::engine_state::UNINITIALIZED) {
        return {false, "No file loaded. Load a file first with 'load <filename>'", 1};
    }

    try {
        if (m_engine->export_to_wav(output_file, sample_rate)) {
            return {true, "✓ Exported to: " + output_file + " at " + std::to_string(sample_rate) + "Hz", 0};
        } else {
            return {false, "Export failed: " + output_file, 1};
        }

    } catch (const std::exception& e) {
        return {false, "Error exporting file: " + std::string(e.what()), 1};
    }
}

// Playback command implementations
nes_cli::command_result nes_cli::cmd_play(const std::vector<std::string>& args) {
    if (m_engine->get_state() == nes_playback_engine::engine_state::UNINITIALIZED) {
        return {false, "No file loaded. Load a file first with 'load <filename>'", 1};
    }

    try {
        if (m_engine->is_playing()) {
            return {true, "Already playing", 0};
        }

        if (m_engine->play()) {
            auto info = m_engine->get_current_music_info();
            return {true, "▶ Playing: " + info.title, 0};
        } else {
            return {false, "Failed to start playback", 1};
        }

    } catch (const std::exception& e) {
        return {false, "Error starting playback: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::cmd_pause(const std::vector<std::string>& args) {
    try {
        if (!m_engine->is_playing()) {
            return {true, "Not currently playing", 0};
        }

        if (m_engine->pause()) {
            return {true, "⏸ Paused", 0};
        } else {
            return {false, "Failed to pause playback", 1};
        }

    } catch (const std::exception& e) {
        return {false, "Error pausing playback: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::cmd_stop(const std::vector<std::string>& args) {
    try {
        if (m_engine->stop()) {
            return {true, "⏹ Stopped", 0};
        } else {
            return {false, "Failed to stop playback", 1};
        }

    } catch (const std::exception& e) {
        return {false, "Error stopping playback: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::cmd_resume(const std::vector<std::string>& args) {
    try {
        if (m_engine->is_playing()) {
            return {true, "Already playing", 0};
        }

        if (m_engine->resume()) {
            return {true, "▶ Resumed", 0};
        } else {
            return {false, "Failed to resume playback", 1};
        }

    } catch (const std::exception& e) {
        return {false, "Error resuming playback: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::cmd_seek(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {false, "Usage: seek <position> (e.g., '1:30', '90s', '50%')", 1};
    }

    std::string position_str = args[0];

    try {
        if (position_str.back() == '%') {
            // Percentage position
            double percentage = std::stod(position_str.substr(0, position_str.length() - 1)) / 100.0;
            if (percentage < 0.0 || percentage > 1.0) {
                return {false, "Percentage must be between 0% and 100%", 1};
            }
            m_engine->seek_to_percentage(percentage);
            return {true, "⏭ Seeked to " + position_str, 0};
        } else {
            // Time position
            double time = nes_cli_utils::parse_time_arg(position_str);
            if (time < 0) {
                return {false, "Invalid time format. Use formats like '1:30', '90s', or '1.5m'", 1};
            }
            music_time_t ticks = static_cast<music_time_t>(time * 480 * m_config.default_tempo_bpm / 60.0);
            m_engine->seek_to_time(ticks);
            return {true, "⏭ Seeked to " + format_duration(time), 0};
        }

    } catch (const std::exception& e) {
        return {false, "Error seeking: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::cmd_status(const std::vector<std::string>& args) {
    try {
        std::ostringstream oss;
        oss << "Playback Status:\n";

        // Engine state
        auto state = m_engine->get_state();
        std::string state_str;
        switch (state) {
            case nes_playback_engine::engine_state::UNINITIALIZED: state_str = "Uninitialized"; break;
            case nes_playback_engine::engine_state::INITIALIZED: state_str = "Initialized"; break;
            case nes_playback_engine::engine_state::LOADING: state_str = "Loading"; break;
            case nes_playback_engine::engine_state::READY: state_str = "Ready"; break;
            case nes_playback_engine::engine_state::PLAYING: state_str = "▶ Playing"; break;
            case nes_playback_engine::engine_state::PAUSED: state_str = "⏸ Paused"; break;
            case nes_playback_engine::engine_state::STOPPING: state_str = "Stopping"; break;
            case nes_playback_engine::engine_state::ERROR: state_str = "Error"; break;
        }
        oss << "  State: " << state_str << "\n";

        if (state != nes_playback_engine::engine_state::UNINITIALIZED) {
            auto info = m_engine->get_current_music_info();
            oss << "  File: " << info.filename << "\n";
            oss << "  Title: " << info.title << "\n";
            oss << "  Duration: " << format_duration(info.duration_seconds) << "\n";

            if (m_engine->is_playing() || state == nes_playback_engine::engine_state::PAUSED) {
                double current_percentage = m_engine->get_current_percentage();
                double current_seconds = current_percentage * info.duration_seconds;
                oss << "  Position: " << format_duration(current_seconds)
                    << " (" << std::fixed << std::setprecision(1) << (current_percentage * 100) << "%)\n";
            }

            oss << "  Tempo Scale: " << m_engine->get_tempo_scale() << "x\n";
            oss << "  Master Volume: " << (m_engine->get_master_volume() * 100) << "%\n";
            oss << "  Loop Enabled: " << (m_engine->is_loop_enabled() ? "yes" : "no") << "\n";
        }

        return {true, oss.str(), 0};

    } catch (const std::exception& e) {
        return {false, "Error getting status: " + std::string(e.what()), 1};
    }
}

// Audio control command implementations
nes_cli::command_result nes_cli::cmd_volume(const std::vector<std::string>& args) {
    if (args.empty()) {
        // Show current volume
        float current_volume = m_engine->get_master_volume();
        return {true, "Master volume: " + std::to_string(static_cast<int>(current_volume * 100)) + "%", 0};
    }

    try {
        std::string volume_str = args[0];
        float volume;

        if (volume_str.back() == '%') {
            volume = std::stof(volume_str.substr(0, volume_str.length() - 1)) / 100.0f;
        } else {
            volume = std::stof(volume_str);
            if (volume > 1.0f) volume /= 100.0f; // Assume percentage if > 1.0
        }

        if (volume < 0.0f || volume > 1.0f) {
            return {false, "Volume must be between 0% and 100%", 1};
        }

        m_engine->set_master_volume(volume);
        m_config.master_volume = volume;

        return {true, "🔊 Volume set to " + std::to_string(static_cast<int>(volume * 100)) + "%", 0};

    } catch (const std::exception& e) {
        return {false, "Error setting volume: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::cmd_tempo(const std::vector<std::string>& args) {
    if (args.empty()) {
        // Show current tempo
        double current_tempo = m_engine->get_tempo_scale();
        return {true, "Tempo scale: " + std::to_string(current_tempo) + "x", 0};
    }

    try {
        std::string tempo_str = args[0];
        double tempo_scale;

        if (tempo_str.back() == 'x') {
            tempo_scale = std::stod(tempo_str.substr(0, tempo_str.length() - 1));
        } else if (tempo_str.back() == '%') {
            tempo_scale = std::stod(tempo_str.substr(0, tempo_str.length() - 1)) / 100.0;
        } else {
            tempo_scale = std::stod(tempo_str);
        }

        if (tempo_scale <= 0.0 || tempo_scale > 4.0) {
            return {false, "Tempo scale must be between 0.1x and 4.0x", 1};
        }

        m_engine->set_tempo_scale(tempo_scale);
        m_config.tempo_scale = tempo_scale;

        return {true, "🎵 Tempo set to " + std::to_string(tempo_scale) + "x", 0};

    } catch (const std::exception& e) {
        return {false, "Error setting tempo: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::cmd_loop(const std::vector<std::string>& args) {
    if (args.empty()) {
        // Show current loop status
        bool loop_enabled = m_engine->is_loop_enabled();
        return {true, "Loop: " + std::string(loop_enabled ? "enabled" : "disabled"), 0};
    }

    try {
        bool enable_loop = nes_cli_utils::parse_bool_arg(args[0]);
        m_engine->set_loop_enabled(enable_loop);
        m_config.enable_looping = enable_loop;

        return {true, "🔁 Loop " + std::string(enable_loop ? "enabled" : "disabled"), 0};

    } catch (const std::exception& e) {
        return {false, "Error setting loop: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::cmd_channels(const std::vector<std::string>& args) {
    // TODO: Implement channel control
    return {false, "Channel control not yet implemented", 1};
}

// NES-specific command implementations
nes_cli::command_result nes_cli::cmd_nes_settings(const std::vector<std::string>& args) {
    if (args.empty()) {
        // Show current NES settings
        std::ostringstream oss;
        oss << "NES APU Settings:\n";
        oss << "  Hardware-accurate mixing: " << (m_config.enable_nonlinear_mixing ? "enabled" : "disabled") << "\n";
        oss << "  High-pass filter (90Hz): " << (m_config.enable_highpass_filter ? "enabled" : "disabled") << "\n";
        oss << "  Low-pass filter (14kHz): " << (m_config.enable_lowpass_filter ? "enabled" : "disabled") << "\n";
        oss << "  Pulse volume scale: " << m_config.pulse_volume_scale << "\n";
        oss << "  Triangle volume scale: " << m_config.triangle_volume_scale << "\n";
        oss << "  Noise volume scale: " << m_config.noise_volume_scale << "\n";
        oss << "  DMC volume scale: " << m_config.dmc_volume_scale << "\n";
        return {true, oss.str(), 0};
    }

    // TODO: Implement NES settings modification
    return {false, "NES settings modification not yet implemented", 1};
}

nes_cli::command_result nes_cli::cmd_nes_channels(const std::vector<std::string>& args) {
    // TODO: Implement NES channel control
    return {false, "NES channel control not yet implemented", 1};
}

nes_cli::command_result nes_cli::cmd_nes_optimize(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {false, "Usage: nes-optimize <filename>", 1};
    }

    std::string filename = args[0];
    if (!validate_file_path(filename)) {
        return {false, "File not found: " + filename, 1};
    }

    try {
        // Load file with NES optimization
        if (m_engine->load_with_nes_optimization(filename)) {
            return {true, "✓ File optimized for NES playback: " + filename, 0};
        } else {
            return {false, "Failed to optimize file: " + filename, 1};
        }

    } catch (const std::exception& e) {
        return {false, "Error optimizing file: " + std::string(e.what()), 1};
    }
}

// Batch command implementations
nes_cli::command_result nes_cli::cmd_batch_validate(const std::vector<std::string>& args) {
    // TODO: Implement batch validation
    return {false, "Batch validation not yet implemented", 1};
}

nes_cli::command_result nes_cli::cmd_batch_convert(const std::vector<std::string>& args) {
    // TODO: Implement batch conversion
    return {false, "Batch conversion not yet implemented", 1};
}

nes_cli::command_result nes_cli::cmd_batch_analyze(const std::vector<std::string>& args) {
    // TODO: Implement batch analysis
    return {false, "Batch analysis not yet implemented", 1};
}

// Information command implementations
nes_cli::command_result nes_cli::cmd_formats(const std::vector<std::string>& args) {
    try {
        auto formats = m_engine->get_supported_file_formats();
        auto extensions = m_file_manager->get_supported_extensions();

        std::ostringstream oss;
        oss << "Supported File Formats:\n";
        for (size_t i = 0; i < formats.size(); ++i) {
            oss << "  " << formats[i] << "\n";
        }

        oss << "\nSupported Extensions:\n  ";
        for (size_t i = 0; i < extensions.size(); ++i) {
            oss << extensions[i];
            if (i < extensions.size() - 1) oss << ", ";
        }
        oss << "\n";

        return {true, oss.str(), 0};

    } catch (const std::exception& e) {
        return {false, "Error getting formats: " + std::string(e.what()), 1};
    }
}

nes_cli::command_result nes_cli::cmd_backends(const std::vector<std::string>& args) {
    std::ostringstream oss;
    oss << "Available Audio Backends:\n";
    oss << "  auto        - Automatically select best backend\n";
    oss << "  alsa        - ALSA (Linux)\n";
    oss << "  directsound - DirectSound (Windows)\n";
    oss << "  file        - File output (no real-time audio)\n";
    oss << "\nCurrent backend: " << m_config.audio_backend << "\n";

    return {true, oss.str(), 0};
}

nes_cli::command_result nes_cli::cmd_metrics(const std::vector<std::string>& args) {
    try {
        auto metrics = m_engine->get_performance_metrics();

        std::ostringstream oss;
        oss << "Performance Metrics:\n";
        oss << "  Audio Performance:\n";
        oss << "    Frames processed: " << metrics.audio_stats.frames_processed << "\n";
        oss << "    Buffer underruns: " << metrics.audio_underruns << "\n";
        oss << "    Buffer overruns: " << metrics.audio_overruns << "\n";
        oss << "    Peak output level: " << std::fixed << std::setprecision(3) << metrics.peak_output_level << "\n";

        oss << "  Sequencer Performance:\n";
        oss << "    Events processed: " << metrics.sequencer_stats.events_processed << "\n";
        oss << "    Notes played: " << metrics.sequencer_stats.notes_played << "\n";
        oss << "    Active voices: " << metrics.active_voices << "\n";
        oss << "    Timing errors: " << metrics.sequencer_stats.timing_errors << "\n";
        oss << "    Average latency: " << std::fixed << std::setprecision(2) << metrics.sequencer_stats.average_latency_ms << " ms\n";

        oss << "  Engine Performance:\n";
        oss << "    Total playback time: " << std::fixed << std::setprecision(1) << (metrics.total_playback_time_ms / 1000.0) << " seconds\n";
        oss << "    Files loaded: " << metrics.files_loaded << "\n";
        oss << "    Playback sessions: " << metrics.playback_sessions << "\n";
        oss << "    Average frame time: " << std::fixed << std::setprecision(2) << metrics.average_frame_time_ms << " ms\n";

        return {true, oss.str(), 0};

    } catch (const std::exception& e) {
        return {false, "Error getting metrics: " + std::string(e.what()), 1};
    }
}

// CLI Application Implementation
std::unique_ptr<nes_cli> nes_cli_app::s_cli;

int nes_cli_app::main(int argc, char* argv[]) {
    try {
        setup_signal_handlers();
        print_startup_banner();

        // Create CLI instance
        s_cli = std::make_unique<nes_cli>();

        // Run CLI
        auto result = s_cli->run(argc, argv);

        // Print result message if any
        if (!result.message.empty()) {
            if (result.success) {
                std::cout << result.message << std::endl;
            } else {
                std::cerr << result.message << std::endl;
            }
        }

        cleanup();
        return result.exit_code;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}

void nes_cli_app::setup_signal_handlers() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

void nes_cli_app::signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully...\n";
    cleanup();
    exit(0);
}

void nes_cli_app::cleanup() {
    if (s_cli) {
        s_cli.reset();
    }
    print_shutdown_message();
}

void nes_cli_app::print_startup_banner() {
    std::cout << "🎵 NES Synthesizer CLI v1.0.0 🎵\n";
    std::cout << "Phase 5: User Interface & Polish\n";
    std::cout << "Built with MAME audio core\n\n";
}

void nes_cli_app::print_shutdown_message() {
    // Only print if we're shutting down gracefully
}

// NES Configuration Command Implementations
nes_cli::command_result nes_cli::cmd_config_load(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {false, "Usage: config-load <filename>", 1};
    }

    std::string filename = args[0];
    if (!validate_file_path(filename, true)) {
        return {false, "Configuration file not found: " + filename, 1};
    }

    nes_config::nes_configuration new_config;
    if (!nes_config::nes_config_manager::load_configuration(filename, new_config)) {
        return {false, "Failed to load configuration from: " + filename, 1};
    }

    std::string error_message;
    if (!nes_config::nes_config_manager::validate_configuration(new_config, error_message)) {
        return {false, "Invalid configuration: " + error_message, 1};
    }

    m_nes_config = new_config;
    sync_config_from_nes_config();
    sync_config_to_engine();

    return {true, "Configuration loaded successfully from: " + filename, 0};
}

nes_cli::command_result nes_cli::cmd_config_save(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {false, "Usage: config-save <filename>", 1};
    }

    std::string filename = args[0];
    if (!nes_config::nes_config_manager::save_configuration(filename, m_nes_config)) {
        return {false, "Failed to save configuration to: " + filename, 1};
    }

    return {true, "Configuration saved successfully to: " + filename, 0};
}

nes_cli::command_result nes_cli::cmd_config_show(const std::vector<std::string>& args) {
    if (args.empty()) {
        print_config_summary();
        return {true, "", 0};
    }

    std::string section = args[0];
    print_config_section(section);
    return {true, "", 0};
}

nes_cli::command_result nes_cli::cmd_config_reset(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    m_nes_config = nes_config::nes_configuration{};
    sync_config_from_nes_config();
    sync_config_to_engine();

    return {true, "Configuration reset to defaults", 0};
}

nes_cli::command_result nes_cli::cmd_config_preset(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {false, "Usage: config-preset <preset>\nAvailable presets: performance, quality, authentic, creative", 1};
    }

    std::string preset = args[0];
    nes_config::nes_configuration new_config;

    if (preset == "performance") {
        new_config = nes_config::nes_config_manager::create_performance_preset();
    } else if (preset == "quality") {
        new_config = nes_config::nes_config_manager::create_quality_preset();
    } else if (preset == "authentic") {
        new_config = nes_config::nes_config_manager::create_authentic_preset();
    } else if (preset == "creative") {
        new_config = nes_config::nes_config_manager::create_creative_preset();
    } else {
        return {false, "Unknown preset: " + preset + "\nAvailable presets: performance, quality, authentic, creative", 1};
    }

    m_nes_config = new_config;
    sync_config_from_nes_config();
    sync_config_to_engine();

    return {true, "Applied '" + preset + "' preset configuration", 0};
}

nes_cli::command_result nes_cli::cmd_config_validate(const std::vector<std::string>& args) {
    if (args.empty()) {
        // Validate current configuration
        std::string error_message;
        bool valid = nes_config::nes_config_manager::validate_configuration(m_nes_config, error_message);
        if (valid) {
            return {true, "Current configuration is valid", 0};
        } else {
            return {false, "Current configuration is invalid: " + error_message, 1};
        }
    }

    // Validate configuration file
    std::string filename = args[0];
    if (!validate_file_path(filename, true)) {
        return {false, "Configuration file not found: " + filename, 1};
    }

    nes_config::nes_configuration config;
    if (!nes_config::nes_config_manager::load_configuration(filename, config)) {
        return {false, "Failed to load configuration from: " + filename, 1};
    }

    std::string error_message;
    bool valid = nes_config::nes_config_manager::validate_configuration(config, error_message);
    if (valid) {
        return {true, "Configuration file is valid: " + filename, 0};
    } else {
        return {false, "Configuration file is invalid: " + error_message, 1};
    }
}

nes_cli::command_result nes_cli::cmd_config_list(const std::vector<std::string>& args) {
    std::string directory = args.empty() ? "config" : args[0];
    auto config_files = nes_config::nes_config_manager::find_configuration_files(directory);

    if (config_files.empty()) {
        return {true, "No configuration files found in: " + directory, 0};
    }

    std::ostringstream oss;
    oss << "Configuration files in " << directory << ":\n";
    for (const auto& file : config_files) {
        oss << "  " << file << "\n";
    }

    return {true, oss.str(), 0};
}

nes_cli::command_result nes_cli::cmd_config_set(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        return {false, "Usage: config-set <section.key> <value>\nExample: config-set audio.sample_rate 48000", 1};
    }

    std::string key_path = args[0];
    std::string value = args[1];

    // Parse section.key format
    size_t dot_pos = key_path.find('.');
    if (dot_pos == std::string::npos) {
        return {false, "Invalid key format. Use section.key (e.g., audio.sample_rate)", 1};
    }

    std::string section = key_path.substr(0, dot_pos);
    std::string key = key_path.substr(dot_pos + 1);

    try {
        // Set configuration values
        if (section == "audio") {
            if (key == "sample_rate") {
                m_nes_config.audio.sample_rate = std::stoul(value);
            } else if (key == "buffer_size") {
                m_nes_config.audio.buffer_size = std::stoul(value);
            } else if (key == "enable_nonlinear_mixing") {
                m_nes_config.audio.enable_nonlinear_mixing = (value == "true" || value == "1");
            } else if (key == "pulse1_volume_scale") {
                m_nes_config.audio.pulse1_volume_scale = std::stof(value);
            } else if (key == "triangle_volume_scale") {
                m_nes_config.audio.triangle_volume_scale = std::stof(value);
            } else if (key == "noise_volume_scale") {
                m_nes_config.audio.noise_volume_scale = std::stof(value);
            } else if (key == "dmc_volume_scale") {
                m_nes_config.audio.dmc_volume_scale = std::stof(value);
            } else {
                return {false, "Unknown audio setting: " + key, 1};
            }
        } else if (section == "performance") {
            if (key == "enable_multithreading") {
                m_nes_config.performance.enable_multithreading = (value == "true" || value == "1");
            } else if (key == "worker_thread_count") {
                m_nes_config.performance.worker_thread_count = std::stoul(value);
            } else if (key == "sample_cache_size_mb") {
                m_nes_config.performance.sample_cache_size_mb = std::stoul(value);
            } else {
                return {false, "Unknown performance setting: " + key, 1};
            }
        } else {
            return {false, "Unknown configuration section: " + section, 1};
        }

        sync_config_from_nes_config();
        sync_config_to_engine();

        return {true, "Set " + key_path + " = " + value, 0};
    } catch (const std::exception& e) {
        return {false, "Invalid value for " + key_path + ": " + e.what(), 1};
    }
}

nes_cli::command_result nes_cli::cmd_config_get(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {false, "Usage: config-get <section.key>\nExample: config-get audio.sample_rate", 1};
    }

    std::string key_path = args[0];

    // Parse section.key format
    size_t dot_pos = key_path.find('.');
    if (dot_pos == std::string::npos) {
        return {false, "Invalid key format. Use section.key (e.g., audio.sample_rate)", 1};
    }

    std::string section = key_path.substr(0, dot_pos);
    std::string key = key_path.substr(dot_pos + 1);

    std::ostringstream oss;
    oss << key_path << " = ";

    // Get configuration values
    if (section == "audio") {
        if (key == "sample_rate") {
            oss << m_nes_config.audio.sample_rate;
        } else if (key == "buffer_size") {
            oss << m_nes_config.audio.buffer_size;
        } else if (key == "enable_nonlinear_mixing") {
            oss << (m_nes_config.audio.enable_nonlinear_mixing ? "true" : "false");
        } else if (key == "pulse1_volume_scale") {
            oss << m_nes_config.audio.pulse1_volume_scale;
        } else if (key == "triangle_volume_scale") {
            oss << m_nes_config.audio.triangle_volume_scale;
        } else if (key == "noise_volume_scale") {
            oss << m_nes_config.audio.noise_volume_scale;
        } else if (key == "dmc_volume_scale") {
            oss << m_nes_config.audio.dmc_volume_scale;
        } else {
            return {false, "Unknown audio setting: " + key, 1};
        }
    } else if (section == "performance") {
        if (key == "enable_multithreading") {
            oss << (m_nes_config.performance.enable_multithreading ? "true" : "false");
        } else if (key == "worker_thread_count") {
            oss << m_nes_config.performance.worker_thread_count;
        } else if (key == "sample_cache_size_mb") {
            oss << m_nes_config.performance.sample_cache_size_mb;
        } else {
            return {false, "Unknown performance setting: " + key, 1};
        }
    } else {
        return {false, "Unknown configuration section: " + section, 1};
    }

    return {true, oss.str(), 0};
}

// Configuration utility methods
void nes_cli::sync_config_to_engine() {
    if (m_engine) {
        nes_playback_engine::engine_config engine_config;
        nes_config::nes_config_manager::apply_to_engine_config(m_nes_config, engine_config);
        m_engine->set_config(engine_config);
    }
}

void nes_cli::sync_config_from_nes_config() {
    nes_config::nes_config_manager::apply_to_cli_config(m_nes_config, m_config);
}

void nes_cli::print_config_summary() const {
    std::cout << "NES Synthesizer Configuration Summary:\n";
    std::cout << "  Config Name: " << m_nes_config.config_name << "\n";
    std::cout << "  Description: " << m_nes_config.description << "\n\n";

    std::cout << "Audio Settings:\n";
    std::cout << "  Sample Rate: " << m_nes_config.audio.sample_rate << " Hz\n";
    std::cout << "  Buffer Size: " << m_nes_config.audio.buffer_size << " frames\n";
    std::cout << "  Nonlinear Mixing: " << (m_nes_config.audio.enable_nonlinear_mixing ? "Enabled" : "Disabled") << "\n";
    std::cout << "  High-pass Filter: " << (m_nes_config.audio.enable_highpass_filter ? "Enabled" : "Disabled") << "\n";
    std::cout << "  Low-pass Filter: " << (m_nes_config.audio.enable_lowpass_filter ? "Enabled" : "Disabled") << "\n\n";

    std::cout << "Channel Volume Scales:\n";
    std::cout << "  Pulse 1: " << m_nes_config.audio.pulse1_volume_scale << "\n";
    std::cout << "  Pulse 2: " << m_nes_config.audio.pulse2_volume_scale << "\n";
    std::cout << "  Triangle: " << m_nes_config.audio.triangle_volume_scale << "\n";
    std::cout << "  Noise: " << m_nes_config.audio.noise_volume_scale << "\n";
    std::cout << "  DMC: " << m_nes_config.audio.dmc_volume_scale << "\n\n";

    std::cout << "Performance Settings:\n";
    std::cout << "  Multithreading: " << (m_nes_config.performance.enable_multithreading ? "Enabled" : "Disabled") << "\n";
    std::cout << "  Worker Threads: " << m_nes_config.performance.worker_thread_count << " (0 = auto)\n";
    std::cout << "  Sample Cache: " << m_nes_config.performance.sample_cache_size_mb << " MB\n";
    std::cout << "  Max Polyphony: " << m_nes_config.performance.max_polyphony << "\n";
}

void nes_cli::print_config_section(const std::string& section) const {
    if (section == "audio") {
        std::cout << "Audio Configuration:\n";
        std::cout << "  sample_rate = " << m_nes_config.audio.sample_rate << "\n";
        std::cout << "  buffer_size = " << m_nes_config.audio.buffer_size << "\n";
        std::cout << "  enable_nonlinear_mixing = " << (m_nes_config.audio.enable_nonlinear_mixing ? "true" : "false") << "\n";
        std::cout << "  enable_highpass_filter = " << (m_nes_config.audio.enable_highpass_filter ? "true" : "false") << "\n";
        std::cout << "  enable_lowpass_filter = " << (m_nes_config.audio.enable_lowpass_filter ? "true" : "false") << "\n";
        std::cout << "  pulse1_volume_scale = " << m_nes_config.audio.pulse1_volume_scale << "\n";
        std::cout << "  pulse2_volume_scale = " << m_nes_config.audio.pulse2_volume_scale << "\n";
        std::cout << "  triangle_volume_scale = " << m_nes_config.audio.triangle_volume_scale << "\n";
        std::cout << "  noise_volume_scale = " << m_nes_config.audio.noise_volume_scale << "\n";
        std::cout << "  dmc_volume_scale = " << m_nes_config.audio.dmc_volume_scale << "\n";
    } else if (section == "performance") {
        std::cout << "Performance Configuration:\n";
        std::cout << "  enable_multithreading = " << (m_nes_config.performance.enable_multithreading ? "true" : "false") << "\n";
        std::cout << "  worker_thread_count = " << m_nes_config.performance.worker_thread_count << "\n";
        std::cout << "  audio_buffer_count = " << m_nes_config.performance.audio_buffer_count << "\n";
        std::cout << "  sample_cache_size_mb = " << m_nes_config.performance.sample_cache_size_mb << "\n";
        std::cout << "  max_polyphony = " << m_nes_config.performance.max_polyphony << "\n";
    } else if (section == "ui") {
        std::cout << "User Interface Configuration:\n";
        std::cout << "  verbose_output = " << (m_nes_config.ui.verbose_output ? "true" : "false") << "\n";
        std::cout << "  use_progress_bars = " << (m_nes_config.ui.use_progress_bars ? "true" : "false") << "\n";
        std::cout << "  enable_colored_output = " << (m_nes_config.ui.enable_colored_output ? "true" : "false") << "\n";
        std::cout << "  log_level = " << m_nes_config.ui.log_level << "\n";
    } else {
        std::cout << "Unknown configuration section: " << section << "\n";
        std::cout << "Available sections: audio, performance, ui\n";
    }
}

// Real-time Control Command Implementations

nes_cli::command_result nes_cli::cmd_realtime_set(const std::vector<std::string>& args) {
    if (!m_realtime_controller) {
        return {false, "Real-time controller not initialized", 1};
    }

    if (args.size() < 2) {
        return {false, "Usage: realtime-set <parameter> [channel] <value>", 1};
    }

    // Map string parameter names to enum values
    std::map<std::string, nes_realtime::parameter_type> param_map = {
        {"master_volume", nes_realtime::parameter_type::MASTER_VOLUME},
        {"master_tempo", nes_realtime::parameter_type::MASTER_TEMPO},
        {"master_pitch", nes_realtime::parameter_type::MASTER_PITCH},
        {"channel_volume", nes_realtime::parameter_type::CHANNEL_VOLUME},
        {"channel_pan", nes_realtime::parameter_type::CHANNEL_PAN},
        {"channel_mute", nes_realtime::parameter_type::CHANNEL_MUTE},
        {"pulse_duty", nes_realtime::parameter_type::PULSE_DUTY_CYCLE},
        {"triangle_linear", nes_realtime::parameter_type::TRIANGLE_LINEAR_COUNTER},
        {"noise_mode", nes_realtime::parameter_type::NOISE_MODE},
        {"highpass_cutoff", nes_realtime::parameter_type::HIGHPASS_CUTOFF},
        {"lowpass_cutoff", nes_realtime::parameter_type::LOWPASS_CUTOFF}
    };

    std::string param_name = args[0];
    auto param_it = param_map.find(param_name);
    if (param_it == param_map.end()) {
        return {false, "Unknown parameter: " + param_name, 1};
    }

    uint8_t channel = 0;
    float value;
    size_t value_index = 1;

    // Check if channel is specified
    if (args.size() >= 3) {
        try {
            channel = static_cast<uint8_t>(std::stoi(args[1]));
            value_index = 2;
        } catch (...) {
            // Not a number, treat as value
        }
    }

    try {
        value = std::stof(args[value_index]);
    } catch (...) {
        return {false, "Invalid value: " + args[value_index], 1};
    }

    if (m_realtime_controller->set_parameter(param_it->second, channel, value, "CLI")) {
        return {true, "Parameter set successfully", 0};
    } else {
        return {false, "Failed to set parameter", 1};
    }
}

nes_cli::command_result nes_cli::cmd_realtime_get(const std::vector<std::string>& args) {
    if (!m_realtime_controller) {
        return {false, "Real-time controller not initialized", 1};
    }

    if (args.empty()) {
        return {false, "Usage: realtime-get <parameter> [channel]", 1};
    }

    // Use same parameter map as set command
    std::map<std::string, nes_realtime::parameter_type> param_map = {
        {"master_volume", nes_realtime::parameter_type::MASTER_VOLUME},
        {"master_tempo", nes_realtime::parameter_type::MASTER_TEMPO},
        {"channel_volume", nes_realtime::parameter_type::CHANNEL_VOLUME},
        {"channel_pan", nes_realtime::parameter_type::CHANNEL_PAN}
    };

    std::string param_name = args[0];
    auto param_it = param_map.find(param_name);
    if (param_it == param_map.end()) {
        return {false, "Unknown parameter: " + param_name, 1};
    }

    uint8_t channel = 0;
    if (args.size() >= 2) {
        try {
            channel = static_cast<uint8_t>(std::stoi(args[1]));
        } catch (...) {
            return {false, "Invalid channel: " + args[1], 1};
        }
    }

    auto param_value = m_realtime_controller->get_parameter(param_it->second, channel);
    std::ostringstream oss;
    oss << param_name << " (channel " << static_cast<int>(channel) << "): "
        << param_value.value << " " << param_value.units;
    return {true, oss.str(), 0};
}

nes_cli::command_result nes_cli::cmd_realtime_preset(const std::vector<std::string>& args) {
    if (!m_realtime_controller) {
        return {false, "Real-time controller not initialized", 1};
    }

    if (args.empty()) {
        return {false, "Usage: realtime-preset <save|load|list> [name]", 1};
    }

    std::string action = args[0];

    if (action == "save") {
        if (args.size() < 2) {
            return {false, "Usage: realtime-preset save <name>", 1};
        }
        m_realtime_controller->save_preset(args[1]);
        return {true, "Preset saved: " + args[1], 0};
    } else if (action == "load") {
        if (args.size() < 2) {
            return {false, "Usage: realtime-preset load <name>", 1};
        }
        if (m_realtime_controller->load_preset(args[1])) {
            return {true, "Preset loaded: " + args[1], 0};
        } else {
            return {false, "Failed to load preset: " + args[1], 1};
        }
    } else if (action == "list") {
        auto presets = m_realtime_controller->get_preset_names();
        std::ostringstream oss;
        oss << "Available presets:\n";
        for (const auto& preset : presets) {
            oss << "  " << preset << "\n";
        }
        return {true, oss.str(), 0};
    } else {
        return {false, "Unknown action: " + action + ". Use save, load, or list.", 1};
    }
}

nes_cli::command_result nes_cli::cmd_realtime_stats(const std::vector<std::string>& args) {
    if (!m_realtime_controller) {
        return {false, "Real-time controller not initialized", 1};
    }

    auto stats = m_realtime_controller->get_stats();
    std::ostringstream oss;
    oss << "Real-time Control Statistics:\n";
    oss << "  Parameters changed: " << stats.parameters_changed << "\n";
    oss << "  MIDI messages processed: " << stats.midi_messages_processed << "\n";
    oss << "  Automation updates: " << stats.automation_updates << "\n";
    oss << "  Average processing time: " << std::fixed << std::setprecision(2)
        << stats.average_processing_time_us << " μs\n";
    oss << "  Active automations: " << stats.active_automations << "\n";
    oss << "  Registered parameters: " << stats.registered_parameters << "\n";

    return {true, oss.str(), 0};
}

nes_cli::command_result nes_cli::cmd_realtime_midi_map(const std::vector<std::string>& args) {
    if (!m_realtime_controller || !m_midi_handler) {
        return {false, "MIDI system not initialized", 1};
    }

    if (args.size() < 3) {
        return {false, "Usage: midi-map <cc_number> <parameter> [channel]", 1};
    }

    try {
        uint8_t cc_number = static_cast<uint8_t>(std::stoi(args[0]));

        // Map parameter name to enum (simplified version)
        nes_realtime::parameter_type param_type = nes_realtime::parameter_type::MASTER_VOLUME;
        if (args[1] == "master_volume") param_type = nes_realtime::parameter_type::MASTER_VOLUME;
        else if (args[1] == "channel_volume") param_type = nes_realtime::parameter_type::CHANNEL_VOLUME;
        else if (args[1] == "channel_pan") param_type = nes_realtime::parameter_type::CHANNEL_PAN;
        else return {false, "Unknown parameter: " + args[1], 1};

        uint8_t channel = args.size() >= 4 ? static_cast<uint8_t>(std::stoi(args[3])) : 0;

        nes_realtime::midi_mapping mapping(cc_number, param_type, channel);
        m_realtime_controller->add_midi_mapping(mapping);

        return {true, "MIDI mapping added: CC" + std::to_string(cc_number) + " -> " + args[1], 0};
    } catch (...) {
        return {false, "Invalid parameters", 1};
    }
}

nes_cli::command_result nes_cli::cmd_realtime_midi_learn(const std::vector<std::string>& args) {
    if (!m_midi_handler) {
        return {false, "MIDI handler not initialized", 1};
    }

    if (args.empty()) {
        return {false, "Usage: midi-learn <parameter> [channel] | midi-learn off", 1};
    }

    if (args[0] == "off") {
        m_midi_handler->exit_learn_mode();
        return {true, "MIDI learn mode disabled", 0};
    }

    // Map parameter name to enum (simplified)
    nes_realtime::parameter_type param_type = nes_realtime::parameter_type::MASTER_VOLUME;
    if (args[0] == "master_volume") param_type = nes_realtime::parameter_type::MASTER_VOLUME;
    else if (args[0] == "channel_volume") param_type = nes_realtime::parameter_type::CHANNEL_VOLUME;
    else return {false, "Unknown parameter: " + args[0], 1};

    uint8_t channel = args.size() >= 2 ? static_cast<uint8_t>(std::stoi(args[1])) : 0;

    m_midi_handler->enter_learn_mode(param_type, channel);
    return {true, "MIDI learn mode enabled for " + args[0] + ". Move a MIDI controller.", 0};
}

nes_cli::command_result nes_cli::cmd_realtime_automation(const std::vector<std::string>& args) {
    if (!m_realtime_controller) {
        return {false, "Real-time controller not initialized", 1};
    }

    if (args.empty()) {
        return {false, "Usage: automation <enable|disable|clear>", 1};
    }

    std::string action = args[0];
    if (action == "enable") {
        m_realtime_controller->set_automation_enabled(true);
        return {true, "Automation enabled", 0};
    } else if (action == "disable") {
        m_realtime_controller->set_automation_enabled(false);
        return {true, "Automation disabled", 0};
    } else if (action == "clear") {
        m_realtime_controller->clear_all_automations();
        return {true, "All automations cleared", 0};
    } else {
        return {false, "Unknown action: " + action, 1};
    }
}

// Convenience command implementations

nes_cli::command_result nes_cli::cmd_channel_volume(const std::vector<std::string>& args) {
    if (!m_realtime_interface) {
        return {false, "Real-time interface not initialized", 1};
    }

    if (args.size() < 2) {
        return {false, "Usage: cv <channel> <volume>", 1};
    }

    try {
        uint8_t channel = static_cast<uint8_t>(std::stoi(args[0]));
        float volume = std::stof(args[1]);
        m_realtime_interface->set_channel_volume(channel, volume);
        return {true, "Channel " + std::to_string(channel) + " volume set to " + args[1], 0};
    } catch (...) {
        return {false, "Invalid parameters", 1};
    }
}

nes_cli::command_result nes_cli::cmd_channel_pan(const std::vector<std::string>& args) {
    if (!m_realtime_interface) {
        return {false, "Real-time interface not initialized", 1};
    }

    if (args.size() < 2) {
        return {false, "Usage: cp <channel> <pan>", 1};
    }

    try {
        uint8_t channel = static_cast<uint8_t>(std::stoi(args[0]));
        float pan = std::stof(args[1]);
        m_realtime_interface->set_channel_pan(channel, pan);
        return {true, "Channel " + std::to_string(channel) + " pan set to " + args[1], 0};
    } catch (...) {
        return {false, "Invalid parameters", 1};
    }
}

nes_cli::command_result nes_cli::cmd_channel_mute(const std::vector<std::string>& args) {
    if (!m_realtime_interface) {
        return {false, "Real-time interface not initialized", 1};
    }

    if (args.size() < 1) {
        return {false, "Usage: cm <channel> [on|off]", 1};
    }

    try {
        uint8_t channel = static_cast<uint8_t>(std::stoi(args[0]));
        bool mute = true;

        if (args.size() >= 2) {
            std::string state = args[1];
            std::transform(state.begin(), state.end(), state.begin(), ::tolower);
            mute = (state == "on" || state == "true" || state == "1");
        }

        m_realtime_interface->mute_channel(channel, mute);
        return {true, "Channel " + std::to_string(channel) + (mute ? " muted" : " unmuted"), 0};
    } catch (...) {
        return {false, "Invalid parameters", 1};
    }
}

nes_cli::command_result nes_cli::cmd_pulse_duty(const std::vector<std::string>& args) {
    if (!m_realtime_interface) {
        return {false, "Real-time interface not initialized", 1};
    }

    if (args.size() < 2) {
        return {false, "Usage: pd <pulse_channel> <duty> (duty: 0-3)", 1};
    }

    try {
        uint8_t channel = static_cast<uint8_t>(std::stoi(args[0]));
        uint8_t duty = static_cast<uint8_t>(std::stoi(args[1]));

        if (duty > 3) {
            return {false, "Duty cycle must be 0-3", 1};
        }

        m_realtime_interface->set_pulse_duty_cycle(channel, duty);
        return {true, "Pulse channel " + std::to_string(channel) + " duty set to " + std::to_string(duty), 0};
    } catch (...) {
        return {false, "Invalid parameters", 1};
    }
}

nes_cli::command_result nes_cli::cmd_filter_cutoff(const std::vector<std::string>& args) {
    if (!m_realtime_interface) {
        return {false, "Real-time interface not initialized", 1};
    }

    if (args.size() < 2) {
        return {false, "Usage: fc <highpass|lowpass> <frequency>", 1};
    }

    try {
        std::string filter_type = args[0];
        float frequency = std::stof(args[1]);

        if (filter_type == "highpass") {
            m_realtime_interface->set_highpass_cutoff(frequency);
        } else if (filter_type == "lowpass") {
            m_realtime_interface->set_lowpass_cutoff(frequency);
        } else {
            return {false, "Unknown filter type: " + filter_type, 1};
        }

        return {true, filter_type + " cutoff set to " + args[1] + " Hz", 0};
    } catch (...) {
        return {false, "Invalid parameters", 1};
    }
}

nes_cli::command_result nes_cli::cmd_master_control(const std::vector<std::string>& args) {
    if (!m_realtime_interface) {
        return {false, "Real-time interface not initialized", 1};
    }

    if (args.size() < 2) {
        return {false, "Usage: mc <volume|tempo|pitch> <value>", 1};
    }

    try {
        std::string control_type = args[0];
        float value = std::stof(args[1]);

        if (control_type == "volume") {
            m_realtime_interface->set_master_volume(value);
        } else if (control_type == "tempo") {
            m_realtime_interface->set_master_tempo(value);
        } else {
            return {false, "Unknown control type: " + control_type, 1};
        }

        return {true, "Master " + control_type + " set to " + args[1], 0};
    } catch (...) {
        return {false, "Invalid parameters", 1};
    }
}

// CLI Utilities Implementation
namespace nes_cli_utils {

std::map<std::string, std::string> parse_key_value_args(const std::vector<std::string>& args) {
    std::map<std::string, std::string> result;
    for (const auto& arg : args) {
        auto pos = arg.find('=');
        if (pos != std::string::npos) {
            std::string key = arg.substr(0, pos);
            std::string value = arg.substr(pos + 1);
            result[key] = value;
        }
    }
    return result;
}

bool parse_bool_arg(const std::string& value) {
    std::string lower_value = value;
    std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);

    return lower_value == "true" || lower_value == "1" || lower_value == "yes" ||
           lower_value == "on" || lower_value == "enable" || lower_value == "enabled";
}

double parse_time_arg(const std::string& value) {
    try {
        // Parse time formats: "1:30", "90s", "1.5m"
        if (value.find(':') != std::string::npos) {
            // MM:SS format
            auto colon_pos = value.find(':');
            int minutes = std::stoi(value.substr(0, colon_pos));
            double seconds = std::stod(value.substr(colon_pos + 1));
            return minutes * 60.0 + seconds;
        } else if (value.back() == 's') {
            // Seconds
            return std::stod(value.substr(0, value.length() - 1));
        } else if (value.back() == 'm') {
            // Minutes
            return std::stod(value.substr(0, value.length() - 1)) * 60.0;
        } else {
            // Assume seconds
            return std::stod(value);
        }
    } catch (...) {
        return -1.0; // Invalid format
    }
}

std::vector<std::string> find_music_files(const std::string& directory, bool recursive) {
    std::vector<std::string> files;
    std::vector<std::string> extensions = {".mid", ".midi", ".xml", ".musicxml", ".mxl", ".nesp", ".nespattern"};

    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) {
                        files.push_back(entry.path().string());
                    }
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) {
                        files.push_back(entry.path().string());
                    }
                }
            }
        }
    } catch (...) {
        // Directory not accessible
    }

    return files;
}

std::string make_backup_filename(const std::string& original) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::filesystem::path path(original);
    std::ostringstream oss;
    oss << path.stem().string() << "_backup_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
        << path.extension().string();

    return path.parent_path() / oss.str();
}

bool is_music_file(const std::string& filename) {
    std::vector<std::string> extensions = {".mid", ".midi", ".xml", ".musicxml", ".mxl", ".nesp", ".nespattern"};
    std::string ext = std::filesystem::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
}

std::string format_table(const std::vector<std::vector<std::string>>& data, const std::vector<std::string>& headers) {
    if (data.empty()) return "";

    // Calculate column widths
    std::vector<size_t> widths;
    size_t num_cols = headers.empty() ? data[0].size() : headers.size();

    for (size_t col = 0; col < num_cols; ++col) {
        size_t max_width = 0;
        if (!headers.empty() && col < headers.size()) {
            max_width = headers[col].length();
        }
        for (const auto& row : data) {
            if (col < row.size()) {
                max_width = std::max(max_width, row[col].length());
            }
        }
        widths.push_back(max_width + 2); // Add padding
    }

    std::ostringstream oss;

    // Print headers
    if (!headers.empty()) {
        for (size_t col = 0; col < headers.size(); ++col) {
            oss << std::left << std::setw(widths[col]) << headers[col];
        }
        oss << "\n";

        // Print separator
        for (size_t col = 0; col < headers.size(); ++col) {
            oss << std::string(widths[col], '-');
        }
        oss << "\n";
    }

    // Print data rows
    for (const auto& row : data) {
        for (size_t col = 0; col < row.size() && col < widths.size(); ++col) {
            oss << std::left << std::setw(widths[col]) << row[col];
        }
        oss << "\n";
    }

    return oss.str();
}

std::string colorize_text(const std::string& text, const std::string& color) {
    // Simple ANSI color codes (disabled for now to avoid terminal compatibility issues)
    return text;
}

std::string format_json(const enhanced_music_metadata& metadata) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"title\": \"" << metadata.title << "\",\n";
    oss << "  \"artist\": \"" << metadata.artist << "\",\n";
    oss << "  \"album\": \"" << metadata.album << "\",\n";
    oss << "  \"format\": \"" << metadata.file_format << "\",\n";
    oss << "  \"duration_seconds\": " << metadata.duration_seconds() << ",\n";
    oss << "  \"total_notes\": " << metadata.total_notes << ",\n";
    oss << "  \"channels_used\": " << metadata.unique_channels_used << ",\n";
    oss << "  \"tempo_bpm\": " << metadata.default_tempo_bpm << ",\n";
    oss << "  \"nes_compatible\": " << (metadata.nes_analysis.is_nes_compatible ? "true" : "false") << "\n";
    oss << "}";
    return oss.str();
}

std::string prompt_user(const std::string& question, const std::string& default_value) {
    std::cout << question;
    if (!default_value.empty()) {
        std::cout << " [" << default_value << "]";
    }
    std::cout << ": ";

    std::string response;
    std::getline(std::cin, response);

    return response.empty() ? default_value : response;
}

bool confirm_user(const std::string& question, bool default_value) {
    std::string default_str = default_value ? "Y/n" : "y/N";
    std::string response = prompt_user(question + " (" + default_str + ")", "");

    if (response.empty()) {
        return default_value;
    }

    char first_char = std::tolower(response[0]);
    return first_char == 'y' || first_char == '1';
}

std::string select_from_list(const std::vector<std::string>& options, const std::string& prompt) {
    std::cout << prompt << "\n";
    for (size_t i = 0; i < options.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << options[i] << "\n";
    }

    std::string response = prompt_user("Select option (1-" + std::to_string(options.size()) + ")", "1");

    try {
        int choice = std::stoi(response);
        if (choice >= 1 && choice <= static_cast<int>(options.size())) {
            return options[choice - 1];
        }
    } catch (...) {
        // Invalid input
    }

    return options.empty() ? "" : options[0];
}

} // namespace nes_cli_utils