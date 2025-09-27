#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>
#include <chrono>
#include <filesystem>
#include "music_parser.h"

/**
 * Comprehensive File Support System for NES Synthesizer
 *
 * Enhanced file format support with:
 * - Advanced metadata extraction and validation
 * - Multiple file format detection and parsing
 * - File conversion capabilities
 * - Batch processing support
 * - Format-specific optimization for NES playback
 * - Detailed error reporting and recovery
 */

// Enhanced metadata with comprehensive music information
struct enhanced_music_metadata {
    // Basic metadata
    std::string title;
    std::string artist;
    std::string album;
    std::string composer;
    std::string arranger;
    std::string copyright;
    std::string comments;
    std::string genre;

    // Technical metadata
    uint16_t ticks_per_quarter = 480;
    uint16_t track_count = 0;
    music_time_t total_ticks = 0;
    uint32_t default_tempo_bpm = 120;
    uint32_t key_signature = 0; // C major = 0
    uint32_t time_signature_numerator = 4;
    uint32_t time_signature_denominator = 4;

    // File metadata
    std::string filename;
    std::string file_format;
    size_t file_size_bytes = 0;
    std::chrono::system_clock::time_point creation_time;
    std::chrono::system_clock::time_point modification_time;

    // Music analysis metadata
    uint32_t total_notes = 0;
    uint32_t unique_channels_used = 0;
    uint32_t tempo_changes = 0;
    uint32_t program_changes = 0;
    uint32_t control_changes = 0;
    music_time_t first_note_time = 0;
    music_time_t last_note_time = 0;

    // NES-specific analysis
    struct nes_compatibility {
        bool is_nes_compatible = false;
        uint8_t pulse1_usage_percentage = 0;  // 0-100
        uint8_t pulse2_usage_percentage = 0;
        uint8_t triangle_usage_percentage = 0;
        uint8_t noise_usage_percentage = 0;
        uint8_t dmc_usage_percentage = 0;
        std::vector<std::string> compatibility_warnings;
        std::vector<std::string> optimization_suggestions;
    } nes_analysis;

    // Duration calculations
    double duration_seconds() const {
        if (total_ticks == 0 || ticks_per_quarter == 0) return 0.0;
        return (double(total_ticks) * 60.0) / (ticks_per_quarter * default_tempo_bpm);
    }

    // Helper functions
    std::string format_duration() const;
    std::string summary_string() const;
};

// File validation result
struct file_validation_result {
    bool is_valid = false;
    std::string format_detected;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<std::string> info_messages;
    enhanced_music_metadata metadata;

    bool has_errors() const { return !errors.empty(); }
    bool has_warnings() const { return !warnings.empty(); }
    std::string get_summary() const;
};

// Advanced file parser interface
class enhanced_music_parser {
public:
    virtual ~enhanced_music_parser() = default;

    // Enhanced parsing with metadata extraction
    virtual bool parse_file_enhanced(const std::string& filename, music_data& output, enhanced_music_metadata& metadata) = 0;

    // File validation without full parsing
    virtual file_validation_result validate_file(const std::string& filename) = 0;

    // Format detection
    virtual bool can_parse_file(const std::string& filename) = 0;
    virtual std::string get_format_name() const = 0;
    virtual std::vector<std::string> get_supported_extensions() const = 0;

    // Conversion capabilities
    virtual bool supports_export() const { return false; }
    virtual bool export_to_file(const music_data& data, const enhanced_music_metadata& metadata, const std::string& filename) { return false; }

    // NES-specific optimization
    virtual bool supports_nes_optimization() const { return false; }
    virtual bool optimize_for_nes(music_data& data, enhanced_music_metadata& metadata) { return false; }
};

// Enhanced MIDI parser with comprehensive metadata support
class enhanced_midi_parser : public enhanced_music_parser {
public:
    enhanced_midi_parser();
    ~enhanced_midi_parser();

    // Enhanced parsing
    bool parse_file_enhanced(const std::string& filename, music_data& output, enhanced_music_metadata& metadata) override;
    file_validation_result validate_file(const std::string& filename) override;

    // Format support
    bool can_parse_file(const std::string& filename) override;
    std::string get_format_name() const override { return "MIDI"; }
    std::vector<std::string> get_supported_extensions() const override { return {".mid", ".midi"}; }

    // Conversion support
    bool supports_export() const override { return true; }
    bool export_to_file(const music_data& data, const enhanced_music_metadata& metadata, const std::string& filename) override;

    // NES optimization
    bool supports_nes_optimization() const override { return true; }
    bool optimize_for_nes(music_data& data, enhanced_music_metadata& metadata) override;

private:
    struct midi_header_info {
        uint16_t format_type = 0;  // 0, 1, or 2
        uint16_t track_count = 0;
        uint16_t ticks_per_quarter = 480;
    };

    struct midi_track_info {
        std::string track_name;
        uint32_t event_count = 0;
        music_time_t duration = 0;
        uint8_t channel_mask = 0; // Bitmask of channels used
    };

    bool parse_midi_header(const std::vector<uint8_t>& data, midi_header_info& header);
    bool parse_midi_track(const uint8_t* data, size_t length, music_data& output,
                         midi_track_info& track_info, music_time_t& current_time);
    void extract_midi_metadata(const std::vector<midi_track_info>& tracks,
                              const midi_header_info& header, enhanced_music_metadata& metadata);
    void analyze_nes_compatibility(const music_data& data, enhanced_music_metadata& metadata);

    // MIDI utility functions
    uint32_t read_variable_length(const uint8_t*& data, size_t& remaining);
    bool read_string_event(const uint8_t* data, size_t length, std::string& result);
};

// Enhanced MusicXML parser
class enhanced_musicxml_parser : public enhanced_music_parser {
public:
    enhanced_musicxml_parser();
    ~enhanced_musicxml_parser();

    // Enhanced parsing
    bool parse_file_enhanced(const std::string& filename, music_data& output, enhanced_music_metadata& metadata) override;
    file_validation_result validate_file(const std::string& filename) override;

    // Format support
    bool can_parse_file(const std::string& filename) override;
    std::string get_format_name() const override { return "MusicXML"; }
    std::vector<std::string> get_supported_extensions() const override { return {".xml", ".musicxml", ".mxl"}; }

    // NES optimization
    bool supports_nes_optimization() const override { return true; }
    bool optimize_for_nes(music_data& data, enhanced_music_metadata& metadata) override;

private:
    bool validate_xml_structure(const std::string& filename, std::vector<std::string>& errors);
    void extract_musicxml_metadata(enhanced_music_metadata& metadata);
};

// Enhanced pattern parser for native NES pattern format
class nes_pattern_parser : public enhanced_music_parser {
public:
    nes_pattern_parser();
    ~nes_pattern_parser();

    // Enhanced parsing
    bool parse_file_enhanced(const std::string& filename, music_data& output, enhanced_music_metadata& metadata) override;
    file_validation_result validate_file(const std::string& filename) override;

    // Format support
    bool can_parse_file(const std::string& filename) override;
    std::string get_format_name() const override { return "NES Pattern"; }
    std::vector<std::string> get_supported_extensions() const override { return {".nesp", ".nespattern"}; }

    // Conversion support
    bool supports_export() const override { return true; }
    bool export_to_file(const music_data& data, const enhanced_music_metadata& metadata, const std::string& filename) override;

    // NES optimization (always optimized)
    bool supports_nes_optimization() const override { return true; }
    bool optimize_for_nes(music_data& data, enhanced_music_metadata& metadata) override { return true; }

private:
    struct pattern_file_header {
        char magic[4] = {'N', 'E', 'S', 'P'};
        uint16_t version = 1;
        uint16_t pattern_count = 0;
        uint32_t ticks_per_quarter = 480;
        uint32_t default_tempo_bpm = 120;
    };
};

// Comprehensive file manager
class comprehensive_file_manager {
public:
    comprehensive_file_manager();
    ~comprehensive_file_manager();

    // Parser registration and management
    void register_parser(std::unique_ptr<enhanced_music_parser> parser);
    std::vector<std::string> get_supported_formats() const;
    std::vector<std::string> get_supported_extensions() const;

    // File operations
    file_validation_result validate_file(const std::string& filename);
    bool load_file(const std::string& filename, music_data& output, enhanced_music_metadata& metadata);
    bool save_file(const std::string& filename, const music_data& data, const enhanced_music_metadata& metadata);

    // Format detection and conversion
    std::string detect_file_format(const std::string& filename);
    bool convert_file(const std::string& input_filename, const std::string& output_filename,
                     const std::string& target_format = "");

    // Batch operations
    struct batch_operation_result {
        std::string filename;
        bool success = false;
        std::string error_message;
        enhanced_music_metadata metadata;
    };

    std::vector<batch_operation_result> validate_directory(const std::string& directory_path,
                                                          bool recursive = false);
    std::vector<batch_operation_result> convert_directory(const std::string& input_directory,
                                                         const std::string& output_directory,
                                                         const std::string& target_format,
                                                         bool recursive = false);

    // NES-specific operations
    struct nes_optimization_result {
        std::string filename;
        bool was_optimized = false;
        std::vector<std::string> changes_made;
        enhanced_music_metadata original_metadata;
        enhanced_music_metadata optimized_metadata;
    };

    nes_optimization_result optimize_file_for_nes(const std::string& filename);
    std::vector<nes_optimization_result> optimize_directory_for_nes(const std::string& directory_path,
                                                                   bool recursive = false);

    // File analysis and reporting
    struct file_analysis_report {
        enhanced_music_metadata metadata;
        std::vector<std::string> technical_details;
        std::vector<std::string> nes_compatibility_notes;
        std::vector<std::string> performance_recommendations;
    };

    file_analysis_report analyze_file(const std::string& filename);
    std::string generate_file_report(const std::string& filename, bool include_nes_analysis = true);

    // Configuration
    struct manager_config {
        bool enable_metadata_caching = true;
        bool enable_nes_analysis = true;
        bool strict_validation = false;
        size_t max_file_size_mb = 100;
        bool enable_backup_on_conversion = true;
    };

    void set_config(const manager_config& config);
    manager_config get_config() const { return m_config; }

private:
    std::vector<std::unique_ptr<enhanced_music_parser>> m_parsers;
    manager_config m_config;

    // Metadata caching
    std::map<std::string, enhanced_music_metadata> m_metadata_cache;
    bool is_metadata_cached(const std::string& filename, enhanced_music_metadata& metadata);
    void cache_metadata(const std::string& filename, const enhanced_music_metadata& metadata);

    // Utility functions
    enhanced_music_parser* find_parser_for_file(const std::string& filename);
    bool is_file_too_large(const std::string& filename);
    std::string generate_backup_filename(const std::string& original_filename);

    // Directory traversal
    void traverse_directory(const std::string& directory_path, bool recursive,
                           std::function<void(const std::string&)> callback);
};

// Utility functions for file format support
namespace file_support_utils {
    // File system utilities
    bool file_exists(const std::string& filename);
    bool is_file_readable(const std::string& filename);
    bool is_file_writable(const std::string& filename);
    size_t get_file_size(const std::string& filename);
    std::chrono::system_clock::time_point get_file_modification_time(const std::string& filename);

    // String utilities
    std::string to_lower(const std::string& str);
    std::string get_file_extension(const std::string& filename);
    std::string remove_extension(const std::string& filename);
    std::string sanitize_filename(const std::string& filename);

    // Music data analysis
    std::vector<uint8_t> get_channels_used(const music_data& data);
    std::pair<music_time_t, music_time_t> get_time_range(const music_data& data);
    uint32_t count_tempo_changes(const music_data& data);

    // NES compatibility analysis
    bool is_note_in_nes_range(uint8_t note);
    uint8_t suggest_nes_channel_for_note(uint8_t note, uint8_t velocity);
    std::vector<std::string> analyze_nes_limitations(const music_data& data);
}

// Factory for creating comprehensive file support
class comprehensive_file_support_factory {
public:
    static std::unique_ptr<comprehensive_file_manager> create_default_manager();
    static std::unique_ptr<comprehensive_file_manager> create_nes_optimized_manager();
    static std::unique_ptr<comprehensive_file_manager> create_minimal_manager();

    // Parser creation
    static std::unique_ptr<enhanced_midi_parser> create_midi_parser();
    static std::unique_ptr<enhanced_musicxml_parser> create_musicxml_parser();
    static std::unique_ptr<nes_pattern_parser> create_pattern_parser();
};