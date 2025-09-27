#include "nes_config.h"
#include "nes_playback_engine.h"
#include "nes_cli.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <regex>
#include <algorithm>
#include <cstring>
#include <cstdlib>

namespace nes_config {

// Configuration Manager Implementation
bool nes_config_manager::load_configuration(const std::string& filename, nes_configuration& config) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        // Determine file format from extension
        std::string extension = std::filesystem::path(filename).extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            content += line + "\n";
        }
        file.close();

        if (extension == ".json") {
            return nes_config_serializer::deserialize_from_json(content, config);
        } else if (extension == ".ini" || extension == ".cfg") {
            return nes_config_serializer::deserialize_from_ini(content, config);
        } else if (extension == ".bin" || extension == ".dat") {
            std::vector<uint8_t> binary_data(content.begin(), content.end());
            return nes_config_serializer::deserialize_from_binary(binary_data, config);
        }

        // Default to JSON format
        return nes_config_serializer::deserialize_from_json(content, config);

    } catch (const std::exception&) {
        return false;
    }
}

bool nes_config_manager::save_configuration(const std::string& filename, const nes_configuration& config) {
    try {
        // Update modification timestamp
        nes_configuration mutable_config = config;
        auto now = std::chrono::system_clock::now();
        mutable_config.modified_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        // Create directory if it doesn't exist
        std::filesystem::create_directories(std::filesystem::path(filename).parent_path());

        std::string extension = std::filesystem::path(filename).extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        std::string content;
        if (extension == ".json") {
            if (!nes_config_serializer::serialize_to_json(mutable_config, content)) {
                return false;
            }
        } else if (extension == ".ini" || extension == ".cfg") {
            if (!nes_config_serializer::serialize_to_ini(mutable_config, content)) {
                return false;
            }
        } else if (extension == ".bin" || extension == ".dat") {
            std::vector<uint8_t> binary_data;
            if (!nes_config_serializer::serialize_to_binary(mutable_config, binary_data)) {
                return false;
            }
            std::ofstream file(filename, std::ios::binary);
            if (!file.is_open()) {
                return false;
            }
            file.write(reinterpret_cast<const char*>(binary_data.data()), binary_data.size());
            return file.good();
        } else {
            // Default to JSON
            if (!nes_config_serializer::serialize_to_json(mutable_config, content)) {
                return false;
            }
        }

        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        file << content;
        return file.good();

    } catch (const std::exception&) {
        return false;
    }
}

bool nes_config_manager::validate_configuration(const nes_configuration& config, std::string& error_message) {
    std::ostringstream errors;

    // Validate audio configuration
    std::string audio_error;
    if (!validate_audio_config(config.audio, audio_error)) {
        errors << "Audio config: " << audio_error << "; ";
    }

    // Validate channel configurations
    for (size_t i = 0; i < config.channels.size(); ++i) {
        std::string channel_error;
        if (!validate_channel_config(config.channels[i], channel_error)) {
            errors << "Channel " << i << ": " << channel_error << "; ";
        }
    }

    // Validate file processing configuration
    std::string file_error;
    if (!validate_file_config(config.file_processing, file_error)) {
        errors << "File config: " << file_error << "; ";
    }

    // Validate performance configuration
    std::string perf_error;
    if (!validate_performance_config(config.performance, perf_error)) {
        errors << "Performance config: " << perf_error << "; ";
    }

    // Check for correct number of channels
    if (config.channels.size() != 5) {
        errors << "Must have exactly 5 channels (Pulse1, Pulse2, Triangle, Noise, DMC); ";
    }

    error_message = errors.str();
    return error_message.empty();
}

void nes_config_manager::apply_safe_defaults(nes_configuration& config) {
    // Reset to safe defaults if values are invalid
    if (config.audio.sample_rate < 8000 || config.audio.sample_rate > 192000) {
        config.audio.sample_rate = 44100;
    }
    if (config.audio.buffer_size < 64 || config.audio.buffer_size > 8192) {
        config.audio.buffer_size = 1024;
    }

    // Clamp volume levels
    for (auto& channel : config.channels) {
        channel.volume = std::clamp(channel.volume, 0.0f, 2.0f);
        channel.pan = std::clamp(channel.pan, -1.0f, 1.0f);
    }

    // Ensure reasonable performance settings
    if (config.performance.worker_thread_count > 16) {
        config.performance.worker_thread_count = 0; // Auto-detect
    }
    if (config.performance.sample_cache_size_mb > 1024) {
        config.performance.sample_cache_size_mb = 16;
    }
}

nes_configuration nes_config_manager::create_performance_preset() {
    nes_configuration config;
    config.config_name = "performance";
    config.description = "Optimized for maximum performance and minimum latency";

    // Performance-oriented audio settings
    config.audio.sample_rate = 44100;
    config.audio.buffer_size = 512;
    config.audio.enable_nonlinear_mixing = false;  // Simpler mixing
    config.audio.enable_anti_aliasing = false;
    config.audio.enable_reverb = false;
    config.audio.quality = nes_audio_config::quality_preset::PERFORMANCE;

    // Optimize performance settings
    config.performance.enable_multithreading = true;
    config.performance.enable_simd_optimization = true;
    config.performance.enable_fast_math = true;
    config.performance.enable_sample_caching = true;
    config.performance.sample_cache_size_mb = 32;

    // Minimal hardware emulation
    config.hardware.enable_cycle_accurate_timing = false;
    config.hardware.enable_frame_accurate_timing = false;
    config.hardware.enable_cartridge_timing = false;

    return config;
}

nes_configuration nes_config_manager::create_quality_preset() {
    nes_configuration config;
    config.config_name = "quality";
    config.description = "Optimized for best audio quality and features";

    // High-quality audio settings
    config.audio.sample_rate = 48000;
    config.audio.buffer_size = 1024;
    config.audio.enable_nonlinear_mixing = true;
    config.audio.enable_anti_aliasing = true;
    config.audio.enable_reverb = true;
    config.audio.reverb_amount = 0.15f;
    config.audio.enable_stereo_separation = true;
    config.audio.stereo_separation_amount = 0.4f;
    config.audio.quality = nes_audio_config::quality_preset::QUALITY;

    // Enhanced hardware emulation
    config.hardware.enable_cycle_accurate_timing = true;
    config.hardware.enable_frame_accurate_timing = true;
    config.hardware.enable_apu_noise_randomization = true;

    // Generous performance budget
    config.performance.sample_cache_size_mb = 64;
    config.performance.enable_sample_caching = true;

    return config;
}

nes_configuration nes_config_manager::create_authentic_preset() {
    nes_configuration config;
    config.config_name = "authentic";
    config.description = "Hardware-accurate NES emulation for authentic sound";

    // Hardware-accurate audio settings
    config.audio.sample_rate = 44100;
    config.audio.buffer_size = 1024;
    config.audio.enable_nonlinear_mixing = true;
    config.audio.enable_dc_blocking = true;
    config.audio.enable_highpass_filter = true;
    config.audio.enable_lowpass_filter = true;
    config.audio.highpass_cutoff_hz = 90.0f;    // Actual NES frequency
    config.audio.lowpass_cutoff_hz = 14000.0f;  // Actual NES frequency
    config.audio.quality = nes_audio_config::quality_preset::AUTHENTIC;

    // Hardware-accurate volume scaling
    config.audio.pulse1_volume_scale = 1.0f;
    config.audio.pulse2_volume_scale = 1.0f;
    config.audio.triangle_volume_scale = 0.9f;
    config.audio.noise_volume_scale = 0.7f;
    config.audio.dmc_volume_scale = 0.5f;

    // Full hardware emulation
    config.hardware.enable_cycle_accurate_timing = true;
    config.hardware.enable_frame_accurate_timing = true;
    config.hardware.enable_apu_noise_randomization = true;
    config.hardware.enable_length_counter_halt = true;
    config.hardware.enable_envelope_looping = true;
    config.hardware.enable_sweep_units = true;

    // No modern enhancements
    config.audio.enable_stereo_separation = false;
    config.audio.enable_reverb = false;
    config.audio.enable_anti_aliasing = false;

    return config;
}

nes_configuration nes_config_manager::create_creative_preset() {
    nes_configuration config;
    config.config_name = "creative";
    config.description = "Enhanced features for modern music creation";

    // Enhanced audio features
    config.audio.sample_rate = 48000;
    config.audio.buffer_size = 1024;
    config.audio.enable_nonlinear_mixing = true;
    config.audio.enable_stereo_separation = true;
    config.audio.stereo_separation_amount = 0.6f;
    config.audio.enable_reverb = true;
    config.audio.reverb_amount = 0.2f;
    config.audio.quality = nes_audio_config::quality_preset::QUALITY;

    // Enhanced channel configurations
    for (auto& channel : config.channels) {
        channel.volume = 1.2f;  // Slightly boosted for modern production
    }

    // Relaxed hardware constraints for creativity
    config.hardware.enable_cycle_accurate_timing = false;
    config.file_processing.strict_nes_compatibility = false;
    config.file_processing.max_polyphony_per_channel = 2; // Allow some polyphony

    return config;
}

template<>
void nes_config_manager::apply_to_engine_config(const nes_configuration& nes_config,
                                               nes_playback_engine::engine_config& engine_config) {
    // Audio settings
    engine_config.sample_rate = nes_config.audio.sample_rate;
    engine_config.buffer_size = nes_config.audio.buffer_size;

    // Performance settings
    engine_config.enable_performance_monitoring = nes_config.performance.enable_performance_monitoring;
    engine_config.max_polyphony = nes_config.performance.max_polyphony;
    engine_config.lookahead_ms = nes_config.performance.lookahead_buffer_ms;

    // File format support
    engine_config.enable_midi_support = nes_config.file_processing.enable_midi_import;
    engine_config.enable_musicxml_support = nes_config.file_processing.enable_musicxml_import;
    engine_config.enable_pattern_support = nes_config.file_processing.enable_pattern_export;

    // Mixer configuration
    engine_config.mixer_config.sample_rate = nes_config.audio.sample_rate;
    engine_config.mixer_config.enable_nonlinear_mixing = nes_config.audio.enable_nonlinear_mixing;
    engine_config.mixer_config.enable_highpass_filter = nes_config.audio.enable_highpass_filter;
    engine_config.mixer_config.enable_lowpass_filter = nes_config.audio.enable_lowpass_filter;
    engine_config.mixer_config.pulse_volume_scale = nes_config.audio.pulse1_volume_scale;
    engine_config.mixer_config.triangle_volume_scale = nes_config.audio.triangle_volume_scale;
    engine_config.mixer_config.noise_volume_scale = nes_config.audio.noise_volume_scale;
    engine_config.mixer_config.dmc_volume_scale = nes_config.audio.dmc_volume_scale;
}

template<>
void nes_config_manager::apply_to_cli_config(const nes_configuration& nes_config,
                                            nes_cli::cli_config& cli_config) {
    // Audio settings
    cli_config.sample_rate = nes_config.audio.sample_rate;
    cli_config.buffer_size = nes_config.audio.buffer_size;

    // NES-specific settings
    cli_config.enable_nonlinear_mixing = nes_config.audio.enable_nonlinear_mixing;
    cli_config.enable_highpass_filter = nes_config.audio.enable_highpass_filter;
    cli_config.enable_lowpass_filter = nes_config.audio.enable_lowpass_filter;
    cli_config.pulse_volume_scale = nes_config.audio.pulse1_volume_scale;
    cli_config.triangle_volume_scale = nes_config.audio.triangle_volume_scale;
    cli_config.noise_volume_scale = nes_config.audio.noise_volume_scale;
    cli_config.dmc_volume_scale = nes_config.audio.dmc_volume_scale;

    // UI settings
    cli_config.verbose = nes_config.ui.verbose_output;
    cli_config.show_progress = nes_config.ui.use_progress_bars;
    cli_config.log_level = nes_config.ui.log_level;

    // File processing
    cli_config.enable_nes_optimization = nes_config.file_processing.auto_optimize_for_nes;
    cli_config.backup_on_conversion = nes_config.file_processing.create_backup_files;
    cli_config.max_file_size_mb = nes_config.file_processing.max_file_size_mb;
}

std::vector<std::string> nes_config_manager::find_configuration_files(const std::string& directory) {
    std::vector<std::string> config_files;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                if (extension == ".json" || extension == ".ini" || extension == ".cfg" || extension == ".bin") {
                    config_files.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception&) {
        // Directory doesn't exist or can't be read
    }

    std::sort(config_files.begin(), config_files.end());
    return config_files;
}

std::string nes_config_manager::get_default_config_path() {
    return "nes_synth_config.json";
}

std::string nes_config_manager::get_user_config_directory() {
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.config/nes_synth/";
    }
    return "./config/";
}

// Private validation methods
bool nes_config_manager::validate_audio_config(const nes_audio_config& config, std::string& error) {
    if (config.sample_rate < 8000 || config.sample_rate > 192000) {
        error = "Sample rate must be between 8000 and 192000 Hz";
        return false;
    }
    if (config.buffer_size < 64 || config.buffer_size > 8192) {
        error = "Buffer size must be between 64 and 8192 samples";
        return false;
    }
    if (config.highpass_cutoff_hz < 1.0f || config.highpass_cutoff_hz > 1000.0f) {
        error = "Highpass cutoff must be between 1 and 1000 Hz";
        return false;
    }
    if (config.lowpass_cutoff_hz < 1000.0f || config.lowpass_cutoff_hz > 24000.0f) {
        error = "Lowpass cutoff must be between 1000 and 24000 Hz";
        return false;
    }
    return true;
}

bool nes_config_manager::validate_channel_config(const nes_channel_config& config, std::string& error) {
    if (config.volume < 0.0f || config.volume > 5.0f) {
        error = "Channel volume must be between 0.0 and 5.0";
        return false;
    }
    if (config.pan < -1.0f || config.pan > 1.0f) {
        error = "Channel pan must be between -1.0 and 1.0";
        return false;
    }
    return true;
}

bool nes_config_manager::validate_file_config(const nes_file_config& config, std::string& error) {
    if (config.max_file_size_mb > 1024) {
        error = "Maximum file size cannot exceed 1024 MB";
        return false;
    }
    if (config.max_polyphony_per_channel > 4) {
        error = "Maximum polyphony per channel cannot exceed 4";
        return false;
    }
    return true;
}

bool nes_config_manager::validate_performance_config(const nes_performance_config& config, std::string& error) {
    if (config.audio_thread_priority > 99) {
        error = "Audio thread priority must be between 0 and 99";
        return false;
    }
    if (config.sample_cache_size_mb > 1024) {
        error = "Sample cache size cannot exceed 1024 MB";
        return false;
    }
    if (config.max_polyphony > 32) {
        error = "Maximum polyphony cannot exceed 32";
        return false;
    }
    return true;
}

// Serializer Implementation - JSON format
bool nes_config_serializer::serialize_to_json(const nes_configuration& config, std::string& json_output) {
    std::ostringstream json;

    json << "{\n";
    json << "  \"config_version\": \"" << config.config_version << "\",\n";
    json << "  \"config_name\": \"" << config.config_name << "\",\n";
    json << "  \"description\": \"" << config.description << "\",\n";
    json << "  \"created_timestamp\": " << config.created_timestamp << ",\n";
    json << "  \"modified_timestamp\": " << config.modified_timestamp << ",\n";

    // Hardware configuration
    json << "  \"hardware\": {\n";
    json << "    \"cpu_clock_rate\": " << config.hardware.cpu_clock_rate << ",\n";
    json << "    \"enable_cycle_accurate_timing\": " << (config.hardware.enable_cycle_accurate_timing ? "true" : "false") << ",\n";
    json << "    \"enable_frame_accurate_timing\": " << (config.hardware.enable_frame_accurate_timing ? "true" : "false") << ",\n";
    json << "    \"region\": \"" << (config.hardware.region == nes_hardware_config::region_type::NTSC ? "NTSC" :
                                   config.hardware.region == nes_hardware_config::region_type::PAL ? "PAL" : "DENDY") << "\"\n";
    json << "  },\n";

    // Audio configuration
    json << "  \"audio\": {\n";
    json << "    \"sample_rate\": " << config.audio.sample_rate << ",\n";
    json << "    \"buffer_size\": " << config.audio.buffer_size << ",\n";
    json << "    \"enable_nonlinear_mixing\": " << (config.audio.enable_nonlinear_mixing ? "true" : "false") << ",\n";
    json << "    \"enable_highpass_filter\": " << (config.audio.enable_highpass_filter ? "true" : "false") << ",\n";
    json << "    \"enable_lowpass_filter\": " << (config.audio.enable_lowpass_filter ? "true" : "false") << ",\n";
    json << "    \"pulse1_volume_scale\": " << config.audio.pulse1_volume_scale << ",\n";
    json << "    \"pulse2_volume_scale\": " << config.audio.pulse2_volume_scale << ",\n";
    json << "    \"triangle_volume_scale\": " << config.audio.triangle_volume_scale << ",\n";
    json << "    \"noise_volume_scale\": " << config.audio.noise_volume_scale << ",\n";
    json << "    \"dmc_volume_scale\": " << config.audio.dmc_volume_scale << ",\n";
    json << "    \"quality\": \"" << (config.audio.quality == nes_audio_config::quality_preset::PERFORMANCE ? "performance" :
                                    config.audio.quality == nes_audio_config::quality_preset::BALANCED ? "balanced" :
                                    config.audio.quality == nes_audio_config::quality_preset::QUALITY ? "quality" : "authentic") << "\"\n";
    json << "  },\n";

    // Channel configurations
    json << "  \"channels\": [\n";
    for (size_t i = 0; i < config.channels.size(); ++i) {
        const auto& channel = config.channels[i];
        json << "    {\n";
        json << "      \"type\": \"" << (channel.type == nes_channel_config::channel_type::PULSE1 ? "pulse1" :
                                        channel.type == nes_channel_config::channel_type::PULSE2 ? "pulse2" :
                                        channel.type == nes_channel_config::channel_type::TRIANGLE ? "triangle" :
                                        channel.type == nes_channel_config::channel_type::NOISE ? "noise" : "dmc") << "\",\n";
        json << "      \"enabled\": " << (channel.enabled ? "true" : "false") << ",\n";
        json << "      \"volume\": " << channel.volume << ",\n";
        json << "      \"pan\": " << channel.pan << ",\n";
        json << "      \"muted\": " << (channel.muted ? "true" : "false") << "\n";
        json << "    }";
        if (i < config.channels.size() - 1) json << ",";
        json << "\n";
    }
    json << "  ],\n";

    // Performance configuration
    json << "  \"performance\": {\n";
    json << "    \"enable_multithreading\": " << (config.performance.enable_multithreading ? "true" : "false") << ",\n";
    json << "    \"worker_thread_count\": " << config.performance.worker_thread_count << ",\n";
    json << "    \"audio_buffer_count\": " << config.performance.audio_buffer_count << ",\n";
    json << "    \"sample_cache_size_mb\": " << config.performance.sample_cache_size_mb << ",\n";
    json << "    \"max_polyphony\": " << config.performance.max_polyphony << "\n";
    json << "  },\n";

    // UI configuration
    json << "  \"ui\": {\n";
    json << "    \"verbose_output\": " << (config.ui.verbose_output ? "true" : "false") << ",\n";
    json << "    \"use_progress_bars\": " << (config.ui.use_progress_bars ? "true" : "false") << ",\n";
    json << "    \"enable_colored_output\": " << (config.ui.enable_colored_output ? "true" : "false") << ",\n";
    json << "    \"log_level\": \"" << config.ui.log_level << "\"\n";
    json << "  }\n";

    json << "}\n";

    json_output = json.str();
    return true;
}

bool nes_config_serializer::deserialize_from_json(const std::string& json_input, nes_configuration& config) {
    // Simple JSON parsing for essential fields
    // In a real implementation, you'd use a proper JSON library like nlohmann/json

    config = nes_configuration{}; // Reset to defaults

    // Extract basic string values
    std::regex config_name_regex("\"config_name\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch match;
    if (std::regex_search(json_input, match, config_name_regex)) {
        config.config_name = match[1].str();
    }

    // Extract boolean values
    std::regex bool_regex("\"([^\"]+)\"\\s*:\\s*(true|false)");
    std::sregex_iterator iter(json_input.begin(), json_input.end(), bool_regex);
    std::sregex_iterator end;

    for (; iter != end; ++iter) {
        std::string key = (*iter)[1].str();
        bool value = (*iter)[2].str() == "true";

        if (key == "enable_nonlinear_mixing") config.audio.enable_nonlinear_mixing = value;
        else if (key == "enable_highpass_filter") config.audio.enable_highpass_filter = value;
        else if (key == "enable_lowpass_filter") config.audio.enable_lowpass_filter = value;
        else if (key == "verbose_output") config.ui.verbose_output = value;
        else if (key == "use_progress_bars") config.ui.use_progress_bars = value;
    }

    // Extract numeric values
    std::regex num_regex("\"([^\"]+)\"\\s*:\\s*(\\d+(?:\\.\\d+)?)");
    iter = std::sregex_iterator(json_input.begin(), json_input.end(), num_regex);

    for (; iter != end; ++iter) {
        std::string key = (*iter)[1].str();
        std::string value_str = (*iter)[2].str();

        if (key == "sample_rate") config.audio.sample_rate = std::stoul(value_str);
        else if (key == "buffer_size") config.audio.buffer_size = std::stoul(value_str);
        else if (key == "pulse1_volume_scale") config.audio.pulse1_volume_scale = std::stof(value_str);
        else if (key == "triangle_volume_scale") config.audio.triangle_volume_scale = std::stof(value_str);
        else if (key == "noise_volume_scale") config.audio.noise_volume_scale = std::stof(value_str);
        else if (key == "dmc_volume_scale") config.audio.dmc_volume_scale = std::stof(value_str);
    }

    return true;
}

// INI format implementation (simplified)
bool nes_config_serializer::serialize_to_ini(const nes_configuration& config, std::string& ini_output) {
    std::ostringstream ini;

    ini << "; NES Synthesizer Configuration\n";
    ini << "; Generated by NES Config Manager\n\n";

    ini << "[general]\n";
    ini << "config_name = " << config.config_name << "\n";
    ini << "description = " << config.description << "\n\n";

    ini << "[audio]\n";
    ini << "sample_rate = " << config.audio.sample_rate << "\n";
    ini << "buffer_size = " << config.audio.buffer_size << "\n";
    ini << "enable_nonlinear_mixing = " << (config.audio.enable_nonlinear_mixing ? "true" : "false") << "\n";
    ini << "enable_highpass_filter = " << (config.audio.enable_highpass_filter ? "true" : "false") << "\n";
    ini << "enable_lowpass_filter = " << (config.audio.enable_lowpass_filter ? "true" : "false") << "\n";
    ini << "pulse1_volume_scale = " << config.audio.pulse1_volume_scale << "\n";
    ini << "pulse2_volume_scale = " << config.audio.pulse2_volume_scale << "\n";
    ini << "triangle_volume_scale = " << config.audio.triangle_volume_scale << "\n";
    ini << "noise_volume_scale = " << config.audio.noise_volume_scale << "\n";
    ini << "dmc_volume_scale = " << config.audio.dmc_volume_scale << "\n\n";

    ini << "[performance]\n";
    ini << "enable_multithreading = " << (config.performance.enable_multithreading ? "true" : "false") << "\n";
    ini << "worker_thread_count = " << config.performance.worker_thread_count << "\n";
    ini << "sample_cache_size_mb = " << config.performance.sample_cache_size_mb << "\n";
    ini << "max_polyphony = " << config.performance.max_polyphony << "\n\n";

    ini << "[ui]\n";
    ini << "verbose_output = " << (config.ui.verbose_output ? "true" : "false") << "\n";
    ini << "use_progress_bars = " << (config.ui.use_progress_bars ? "true" : "false") << "\n";
    ini << "log_level = " << config.ui.log_level << "\n";

    ini_output = ini.str();
    return true;
}

bool nes_config_serializer::deserialize_from_ini(const std::string& ini_input, nes_configuration& config) {
    config = nes_configuration{}; // Reset to defaults

    std::istringstream stream(ini_input);
    std::string line;
    std::string current_section;

    while (std::getline(stream, line)) {
        // Remove comments and trim whitespace
        size_t comment_pos = line.find(';');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        if (line.empty()) continue;

        // Check for section headers
        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }

        // Parse key=value pairs
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);

        // Trim key and value
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // Apply values based on section and key
        if (current_section == "general") {
            if (key == "config_name") config.config_name = value;
            else if (key == "description") config.description = value;
        } else if (current_section == "audio") {
            if (key == "sample_rate") config.audio.sample_rate = std::stoul(value);
            else if (key == "buffer_size") config.audio.buffer_size = std::stoul(value);
            else if (key == "enable_nonlinear_mixing") config.audio.enable_nonlinear_mixing = (value == "true");
            else if (key == "enable_highpass_filter") config.audio.enable_highpass_filter = (value == "true");
            else if (key == "enable_lowpass_filter") config.audio.enable_lowpass_filter = (value == "true");
            else if (key == "pulse1_volume_scale") config.audio.pulse1_volume_scale = std::stof(value);
            else if (key == "triangle_volume_scale") config.audio.triangle_volume_scale = std::stof(value);
            else if (key == "noise_volume_scale") config.audio.noise_volume_scale = std::stof(value);
            else if (key == "dmc_volume_scale") config.audio.dmc_volume_scale = std::stof(value);
        } else if (current_section == "performance") {
            if (key == "enable_multithreading") config.performance.enable_multithreading = (value == "true");
            else if (key == "worker_thread_count") config.performance.worker_thread_count = std::stoul(value);
            else if (key == "sample_cache_size_mb") config.performance.sample_cache_size_mb = std::stoul(value);
            else if (key == "max_polyphony") config.performance.max_polyphony = std::stoul(value);
        } else if (current_section == "ui") {
            if (key == "verbose_output") config.ui.verbose_output = (value == "true");
            else if (key == "use_progress_bars") config.ui.use_progress_bars = (value == "true");
            else if (key == "log_level") config.ui.log_level = value;
        }
    }

    return true;
}

// Binary format (simplified implementation)
bool nes_config_serializer::serialize_to_binary(const nes_configuration& config, std::vector<uint8_t>& binary_output) {
    binary_output.clear();

    // Simple binary format: magic number + version + data
    uint32_t magic = 0x4E455343; // "NESC"
    uint32_t version = 1;

    // Write magic and version
    binary_output.resize(8);
    memcpy(binary_output.data(), &magic, 4);
    memcpy(binary_output.data() + 4, &version, 4);

    // Write basic configuration data
    size_t offset = 8;
    binary_output.resize(offset + sizeof(uint32_t) * 4 + sizeof(float) * 4 + 1);

    memcpy(binary_output.data() + offset, &config.audio.sample_rate, sizeof(uint32_t)); offset += 4;
    memcpy(binary_output.data() + offset, &config.audio.buffer_size, sizeof(uint32_t)); offset += 4;
    memcpy(binary_output.data() + offset, &config.audio.pulse1_volume_scale, sizeof(float)); offset += 4;
    memcpy(binary_output.data() + offset, &config.audio.triangle_volume_scale, sizeof(float)); offset += 4;
    memcpy(binary_output.data() + offset, &config.audio.noise_volume_scale, sizeof(float)); offset += 4;
    memcpy(binary_output.data() + offset, &config.audio.dmc_volume_scale, sizeof(float)); offset += 4;

    uint8_t flags = 0;
    if (config.audio.enable_nonlinear_mixing) flags |= 0x01;
    if (config.audio.enable_highpass_filter) flags |= 0x02;
    if (config.audio.enable_lowpass_filter) flags |= 0x04;
    binary_output[offset] = flags;

    return true;
}

bool nes_config_serializer::deserialize_from_binary(const std::vector<uint8_t>& binary_input, nes_configuration& config) {
    if (binary_input.size() < 8) return false;

    uint32_t magic, version;
    memcpy(&magic, binary_input.data(), 4);
    memcpy(&version, binary_input.data() + 4, 4);

    if (magic != 0x4E455343 || version != 1) return false;

    config = nes_configuration{}; // Reset to defaults

    if (binary_input.size() < 33) return false; // Basic size check

    size_t offset = 8;
    memcpy(&config.audio.sample_rate, binary_input.data() + offset, sizeof(uint32_t)); offset += 4;
    memcpy(&config.audio.buffer_size, binary_input.data() + offset, sizeof(uint32_t)); offset += 4;
    memcpy(&config.audio.pulse1_volume_scale, binary_input.data() + offset, sizeof(float)); offset += 4;
    memcpy(&config.audio.triangle_volume_scale, binary_input.data() + offset, sizeof(float)); offset += 4;
    memcpy(&config.audio.noise_volume_scale, binary_input.data() + offset, sizeof(float)); offset += 4;
    memcpy(&config.audio.dmc_volume_scale, binary_input.data() + offset, sizeof(float)); offset += 4;

    uint8_t flags = binary_input[offset];
    config.audio.enable_nonlinear_mixing = (flags & 0x01) != 0;
    config.audio.enable_highpass_filter = (flags & 0x02) != 0;
    config.audio.enable_lowpass_filter = (flags & 0x04) != 0;

    return true;
}

// nes_config_serializer implementation
bool nes_config_serializer::merge_configurations(const nes_configuration& base_config,
                                               const nes_configuration& overlay_config,
                                               nes_configuration& result_config) {
    try {
        // Start with base configuration
        result_config = base_config;

        // Merge overlay configuration over base
        // Note: This is a simple field-by-field merge. In a real implementation,
        // you might want more sophisticated merging logic.

        // Only override non-default values from overlay
        if (overlay_config.config_version != "1.0") {
            result_config.config_version = overlay_config.config_version;
        }

        if (overlay_config.config_name != "default") {
            result_config.config_name = overlay_config.config_name;
        }

        // Audio settings - merge if overlay has non-default values
        if (overlay_config.audio.sample_rate != 44100) {
            result_config.audio.sample_rate = overlay_config.audio.sample_rate;
        }

        if (overlay_config.audio.buffer_size != 1024) {
            result_config.audio.buffer_size = overlay_config.audio.buffer_size;
        }

        // For boolean flags, overlay always takes precedence if explicitly set
        result_config.audio.enable_nonlinear_mixing = overlay_config.audio.enable_nonlinear_mixing;
        result_config.audio.enable_highpass_filter = overlay_config.audio.enable_highpass_filter;
        result_config.audio.enable_lowpass_filter = overlay_config.audio.enable_lowpass_filter;

        // Volume settings (per-channel volume scales)
        if (overlay_config.audio.pulse1_volume_scale != 1.0f) {
            result_config.audio.pulse1_volume_scale = overlay_config.audio.pulse1_volume_scale;
        }
        if (overlay_config.audio.pulse2_volume_scale != 1.0f) {
            result_config.audio.pulse2_volume_scale = overlay_config.audio.pulse2_volume_scale;
        }
        if (overlay_config.audio.triangle_volume_scale != 0.9f) {
            result_config.audio.triangle_volume_scale = overlay_config.audio.triangle_volume_scale;
        }

        // Channel settings - merge channel by channel
        for (size_t i = 0; i < std::min(result_config.channels.size(), overlay_config.channels.size()); ++i) {
            if (overlay_config.channels[i].volume != 1.0f) {
                result_config.channels[i].volume = overlay_config.channels[i].volume;
            }
            if (overlay_config.channels[i].pan != 0.0f) {
                result_config.channels[i].pan = overlay_config.channels[i].pan;
            }
            // enabled flag always takes precedence from overlay
            result_config.channels[i].enabled = overlay_config.channels[i].enabled;
        }

        // Hardware settings
        if (overlay_config.hardware.cpu_clock_rate != 1789773) {
            result_config.hardware.cpu_clock_rate = overlay_config.hardware.cpu_clock_rate;
        }

        // Performance settings
        if (overlay_config.performance.max_polyphony != 32) {
            result_config.performance.max_polyphony = overlay_config.performance.max_polyphony;
        }

        if (overlay_config.performance.lookahead_buffer_ms != 100) {
            result_config.performance.lookahead_buffer_ms = overlay_config.performance.lookahead_buffer_ms;
        }

        result_config.performance.enable_performance_monitoring = overlay_config.performance.enable_performance_monitoring;

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace nes_config