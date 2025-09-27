#include "comprehensive_file_support.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>

// Helper function implementations
std::string enhanced_music_metadata::format_duration() const {
    double seconds = duration_seconds();
    int minutes = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    int centiseconds = static_cast<int>((seconds - static_cast<int>(seconds)) * 100);

    std::ostringstream oss;
    oss << minutes << ":" << std::setfill('0') << std::setw(2) << secs
        << "." << std::setw(2) << centiseconds;
    return oss.str();
}

std::string enhanced_music_metadata::summary_string() const {
    std::ostringstream oss;
    oss << title;
    if (!artist.empty()) oss << " by " << artist;
    if (!album.empty()) oss << " from " << album;
    oss << " (" << format_duration() << ")";
    return oss.str();
}

std::string file_validation_result::get_summary() const {
    std::ostringstream oss;
    oss << "Format: " << format_detected << ", Valid: " << (is_valid ? "Yes" : "No");
    if (has_errors()) oss << ", Errors: " << errors.size();
    if (has_warnings()) oss << ", Warnings: " << warnings.size();
    return oss.str();
}

// Enhanced MIDI Parser Implementation
enhanced_midi_parser::enhanced_midi_parser() = default;
enhanced_midi_parser::~enhanced_midi_parser() = default;

bool enhanced_midi_parser::parse_file_enhanced(const std::string& filename, music_data& output, enhanced_music_metadata& metadata) {
    // Read file
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read all data
    std::vector<uint8_t> data(file_size);
    file.read(reinterpret_cast<char*>(data.data()), file_size);
    file.close();

    // Parse MIDI header
    midi_header_info header;
    if (!parse_midi_header(data, header)) {
        return false;
    }

    // Initialize metadata
    metadata.filename = filename;
    metadata.file_format = "MIDI";
    metadata.file_size_bytes = file_size;
    metadata.ticks_per_quarter = header.ticks_per_quarter;
    metadata.track_count = header.track_count;
    metadata.creation_time = file_support_utils::get_file_modification_time(filename);
    metadata.modification_time = metadata.creation_time;

    // Parse tracks
    std::vector<midi_track_info> track_infos;
    const uint8_t* current_data = data.data() + 14; // Skip header
    size_t remaining_data = data.size() - 14;
    music_time_t global_time = 0;

    for (uint16_t track = 0; track < header.track_count && remaining_data > 8; ++track) {
        // Check track header
        if (remaining_data < 8 ||
            current_data[0] != 'M' || current_data[1] != 'T' ||
            current_data[2] != 'r' || current_data[3] != 'k') {
            break;
        }

        // Get track length
        uint32_t track_length = (current_data[4] << 24) | (current_data[5] << 16) |
                               (current_data[6] << 8) | current_data[7];
        current_data += 8;
        remaining_data -= 8;

        if (track_length > remaining_data) {
            break;
        }

        // Parse track
        midi_track_info track_info;
        music_time_t track_time = global_time;
        if (parse_midi_track(current_data, track_length, output, track_info, track_time)) {
            track_infos.push_back(track_info);
        }

        current_data += track_length;
        remaining_data -= track_length;
        global_time = std::max(global_time, track_time);
    }

    // Extract metadata from tracks
    extract_midi_metadata(track_infos, header, metadata);

    // Analyze NES compatibility
    analyze_nes_compatibility(output, metadata);

    return true;
}

file_validation_result enhanced_midi_parser::validate_file(const std::string& filename) {
    file_validation_result result;
    result.format_detected = "MIDI";

    // Check file existence and readability
    if (!file_support_utils::file_exists(filename)) {
        result.errors.push_back("File does not exist");
        return result;
    }

    if (!file_support_utils::is_file_readable(filename)) {
        result.errors.push_back("File is not readable");
        return result;
    }

    // Check file size
    size_t file_size = file_support_utils::get_file_size(filename);
    if (file_size < 14) {
        result.errors.push_back("File too small to be a valid MIDI file");
        return result;
    }

    if (file_size > 100 * 1024 * 1024) { // 100MB limit
        result.warnings.push_back("Large MIDI file may take time to process");
    }

    // Read and validate header
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        result.errors.push_back("Cannot open file for reading");
        return result;
    }

    std::vector<uint8_t> header_data(14);
    file.read(reinterpret_cast<char*>(header_data.data()), 14);
    file.close();

    // Validate MIDI header
    if (header_data[0] != 'M' || header_data[1] != 'T' ||
        header_data[2] != 'h' || header_data[3] != 'd') {
        result.errors.push_back("Invalid MIDI header signature");
        return result;
    }

    uint32_t header_length = (header_data[4] << 24) | (header_data[5] << 16) |
                            (header_data[6] << 8) | header_data[7];
    if (header_length != 6) {
        result.errors.push_back("Invalid MIDI header length");
        return result;
    }

    uint16_t format_type = (header_data[8] << 8) | header_data[9];
    if (format_type > 2) {
        result.errors.push_back("Unsupported MIDI format type");
        return result;
    }

    uint16_t track_count = (header_data[10] << 8) | header_data[11];
    if (track_count == 0) {
        result.warnings.push_back("MIDI file has no tracks");
    } else if (track_count > 64) {
        result.warnings.push_back("Large number of tracks may impact performance");
    }

    uint16_t ticks_per_quarter = (header_data[12] << 8) | header_data[13];
    if (ticks_per_quarter == 0) {
        result.errors.push_back("Invalid ticks per quarter note");
        return result;
    }

    result.is_valid = true;
    result.info_messages.push_back("Valid MIDI file format " + std::to_string(format_type));
    result.info_messages.push_back(std::to_string(track_count) + " tracks, " +
                                  std::to_string(ticks_per_quarter) + " ticks per quarter");

    return result;
}

bool enhanced_midi_parser::can_parse_file(const std::string& filename) {
    std::string ext = file_support_utils::to_lower(file_support_utils::get_file_extension(filename));
    return ext == ".mid" || ext == ".midi";
}

bool enhanced_midi_parser::export_to_file(const music_data& data, const enhanced_music_metadata& metadata, const std::string& filename) {
    // TODO: Implement MIDI export functionality
    return false;
}

bool enhanced_midi_parser::optimize_for_nes(music_data& data, enhanced_music_metadata& metadata) {
    // TODO: Implement NES optimization (channel limiting, note range adjustment, etc.)
    return false;
}

bool enhanced_midi_parser::parse_midi_header(const std::vector<uint8_t>& data, midi_header_info& header) {
    if (data.size() < 14) return false;

    // Validate header
    if (data[0] != 'M' || data[1] != 'T' || data[2] != 'h' || data[3] != 'd') {
        return false;
    }

    uint32_t header_length = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    if (header_length != 6) return false;

    header.format_type = (data[8] << 8) | data[9];
    header.track_count = (data[10] << 8) | data[11];
    header.ticks_per_quarter = (data[12] << 8) | data[13];

    return header.format_type <= 2 && header.track_count > 0 && header.ticks_per_quarter > 0;
}

bool enhanced_midi_parser::parse_midi_track(const uint8_t* data, size_t length, music_data& output,
                                           midi_track_info& track_info, music_time_t& current_time) {
    // Basic track parsing - simplified implementation
    const uint8_t* ptr = data;
    size_t remaining = length;
    uint8_t running_status = 0;

    while (remaining > 0) {
        // Read delta time
        uint32_t delta_time = read_variable_length(ptr, remaining);
        current_time += delta_time;

        if (remaining == 0) break;

        uint8_t status = *ptr;
        if (status & 0x80) {
            // New status byte
            running_status = status;
            ptr++;
            remaining--;
        } else {
            // Use running status
            status = running_status;
        }

        if (remaining == 0) break;

        // Parse event based on status
        if ((status & 0xF0) == 0x90) {
            // Note on
            if (remaining >= 2) {
                uint8_t note = ptr[0];
                uint8_t velocity = ptr[1];
                uint8_t channel = status & 0x0F;

                if (velocity > 0) {
                    output.add_note(music_note(channel, note, velocity, current_time, 480));
                    track_info.event_count++;
                    track_info.channel_mask |= (1 << channel);
                }

                ptr += 2;
                remaining -= 2;
            }
        } else if ((status & 0xF0) == 0x80) {
            // Note off
            if (remaining >= 2) {
                ptr += 2;
                remaining -= 2;
                track_info.event_count++;
            }
        } else if (status == 0xFF) {
            // Meta event
            if (remaining >= 2) {
                uint8_t meta_type = ptr[0];
                ptr++;
                remaining--;

                uint32_t length = read_variable_length(ptr, remaining);
                if (length <= remaining) {
                    if (meta_type == 0x03 && track_info.track_name.empty()) {
                        // Track name
                        track_info.track_name = std::string(reinterpret_cast<const char*>(ptr), length);
                    }
                    ptr += length;
                    remaining -= length;
                }
            }
        } else {
            // Skip other events
            if (remaining > 0) {
                ptr++;
                remaining--;
            }
        }
    }

    track_info.duration = current_time;
    return true;
}

void enhanced_midi_parser::extract_midi_metadata(const std::vector<midi_track_info>& tracks,
                                                const midi_header_info& header, enhanced_music_metadata& metadata) {
    metadata.track_count = header.track_count;
    metadata.ticks_per_quarter = header.ticks_per_quarter;

    // Calculate total duration and analyze tracks
    music_time_t max_duration = 0;
    uint32_t total_notes = 0;
    uint8_t all_channels = 0;

    for (const auto& track : tracks) {
        max_duration = std::max(max_duration, track.duration);
        total_notes += track.event_count;
        all_channels |= track.channel_mask;

        if (!track.track_name.empty() && metadata.title.empty()) {
            metadata.title = track.track_name;
        }
    }

    metadata.total_ticks = max_duration;
    metadata.total_notes = total_notes;
    metadata.unique_channels_used = __builtin_popcount(all_channels);

    // Set default values
    if (metadata.title.empty()) {
        metadata.title = file_support_utils::remove_extension(
            std::filesystem::path(metadata.filename).filename().string());
    }
}

void enhanced_midi_parser::analyze_nes_compatibility(const music_data& data, enhanced_music_metadata& metadata) {
    auto& nes = metadata.nes_analysis;

    // Basic NES compatibility analysis
    nes.is_nes_compatible = (metadata.unique_channels_used <= 5);

    if (!nes.is_nes_compatible) {
        nes.compatibility_warnings.push_back("Uses more than 5 channels (NES APU has 5 channels)");
    }

    if (metadata.total_notes > 1000) {
        nes.optimization_suggestions.push_back("Consider simplifying arrangement for authentic NES sound");
    }

    // Channel usage analysis (simplified)
    for (int i = 0; i < 5; ++i) {
        if (i < metadata.unique_channels_used) {
            switch (i) {
                case 0: nes.pulse1_usage_percentage = 80; break;
                case 1: nes.pulse2_usage_percentage = 60; break;
                case 2: nes.triangle_usage_percentage = 40; break;
                case 3: nes.noise_usage_percentage = 20; break;
                case 4: nes.dmc_usage_percentage = 10; break;
            }
        }
    }
}

uint32_t enhanced_midi_parser::read_variable_length(const uint8_t*& data, size_t& remaining) {
    uint32_t value = 0;
    uint8_t byte;

    do {
        if (remaining == 0) return 0;
        byte = *data++;
        remaining--;
        value = (value << 7) | (byte & 0x7F);
    } while (byte & 0x80);

    return value;
}

bool enhanced_midi_parser::read_string_event(const uint8_t* data, size_t length, std::string& result) {
    result = std::string(reinterpret_cast<const char*>(data), length);
    return true;
}

// Enhanced MusicXML Parser Implementation
enhanced_musicxml_parser::enhanced_musicxml_parser() = default;
enhanced_musicxml_parser::~enhanced_musicxml_parser() = default;

bool enhanced_musicxml_parser::parse_file_enhanced(const std::string& filename, music_data& output, enhanced_music_metadata& metadata) {
    // TODO: Implement MusicXML parsing
    metadata.filename = filename;
    metadata.file_format = "MusicXML";
    metadata.file_size_bytes = file_support_utils::get_file_size(filename);
    return false;
}

file_validation_result enhanced_musicxml_parser::validate_file(const std::string& filename) {
    file_validation_result result;
    result.format_detected = "MusicXML";

    if (!file_support_utils::file_exists(filename)) {
        result.errors.push_back("File does not exist");
        return result;
    }

    std::vector<std::string> xml_errors;
    if (!validate_xml_structure(filename, xml_errors)) {
        result.errors = xml_errors;
        return result;
    }

    result.is_valid = true;
    return result;
}

bool enhanced_musicxml_parser::can_parse_file(const std::string& filename) {
    std::string ext = file_support_utils::to_lower(file_support_utils::get_file_extension(filename));
    return ext == ".xml" || ext == ".musicxml" || ext == ".mxl";
}

bool enhanced_musicxml_parser::optimize_for_nes(music_data& data, enhanced_music_metadata& metadata) {
    // TODO: Implement NES optimization for MusicXML
    return false;
}

bool enhanced_musicxml_parser::validate_xml_structure(const std::string& filename, std::vector<std::string>& errors) {
    // Basic XML validation - simplified
    std::ifstream file(filename);
    if (!file.is_open()) {
        errors.push_back("Cannot open file");
        return false;
    }

    std::string line;
    bool found_xml_declaration = false;
    while (std::getline(file, line)) {
        if (line.find("<?xml") != std::string::npos) {
            found_xml_declaration = true;
            break;
        }
    }

    if (!found_xml_declaration) {
        errors.push_back("No XML declaration found");
        return false;
    }

    return true;
}

void enhanced_musicxml_parser::extract_musicxml_metadata(enhanced_music_metadata& metadata) {
    // TODO: Extract metadata from parsed MusicXML
}

// NES Pattern Parser Implementation
nes_pattern_parser::nes_pattern_parser() = default;
nes_pattern_parser::~nes_pattern_parser() = default;

bool nes_pattern_parser::parse_file_enhanced(const std::string& filename, music_data& output, enhanced_music_metadata& metadata) {
    // TODO: Implement NES pattern parsing
    metadata.filename = filename;
    metadata.file_format = "NES Pattern";
    metadata.file_size_bytes = file_support_utils::get_file_size(filename);
    return false;
}

file_validation_result nes_pattern_parser::validate_file(const std::string& filename) {
    file_validation_result result;
    result.format_detected = "NES Pattern";

    if (!file_support_utils::file_exists(filename)) {
        result.errors.push_back("File does not exist");
        return result;
    }

    // TODO: Validate NES pattern file format
    result.is_valid = true;
    return result;
}

bool nes_pattern_parser::can_parse_file(const std::string& filename) {
    std::string ext = file_support_utils::to_lower(file_support_utils::get_file_extension(filename));
    return ext == ".nesp" || ext == ".nespattern";
}

bool nes_pattern_parser::export_to_file(const music_data& data, const enhanced_music_metadata& metadata, const std::string& filename) {
    // TODO: Implement NES pattern export
    return false;
}

// Comprehensive File Manager Implementation
comprehensive_file_manager::comprehensive_file_manager() = default;
comprehensive_file_manager::~comprehensive_file_manager() = default;

void comprehensive_file_manager::register_parser(std::unique_ptr<enhanced_music_parser> parser) {
    m_parsers.push_back(std::move(parser));
}

std::vector<std::string> comprehensive_file_manager::get_supported_formats() const {
    std::vector<std::string> formats;
    for (const auto& parser : m_parsers) {
        formats.push_back(parser->get_format_name());
    }
    return formats;
}

std::vector<std::string> comprehensive_file_manager::get_supported_extensions() const {
    std::vector<std::string> extensions;
    for (const auto& parser : m_parsers) {
        auto parser_exts = parser->get_supported_extensions();
        extensions.insert(extensions.end(), parser_exts.begin(), parser_exts.end());
    }
    return extensions;
}

file_validation_result comprehensive_file_manager::validate_file(const std::string& filename) {
    auto* parser = find_parser_for_file(filename);
    if (!parser) {
        file_validation_result result;
        result.format_detected = "Unknown";
        result.errors.push_back("No parser available for this file type");
        return result;
    }

    return parser->validate_file(filename);
}

bool comprehensive_file_manager::load_file(const std::string& filename, music_data& output, enhanced_music_metadata& metadata) {
    // Check cache first
    if (m_config.enable_metadata_caching && is_metadata_cached(filename, metadata)) {
        // Still need to parse the actual music data
        auto* parser = find_parser_for_file(filename);
        if (parser) {
            enhanced_music_metadata temp_metadata;
            return parser->parse_file_enhanced(filename, output, temp_metadata);
        }
        return false;
    }

    auto* parser = find_parser_for_file(filename);
    if (!parser) {
        return false;
    }

    if (is_file_too_large(filename)) {
        return false;
    }

    bool success = parser->parse_file_enhanced(filename, output, metadata);

    if (success && m_config.enable_metadata_caching) {
        cache_metadata(filename, metadata);
    }

    return success;
}

bool comprehensive_file_manager::save_file(const std::string& filename, const music_data& data, const enhanced_music_metadata& metadata) {
    auto* parser = find_parser_for_file(filename);
    if (!parser || !parser->supports_export()) {
        return false;
    }

    if (m_config.enable_backup_on_conversion && file_support_utils::file_exists(filename)) {
        std::string backup_name = generate_backup_filename(filename);
        std::filesystem::copy_file(filename, backup_name);
    }

    return parser->export_to_file(data, metadata, filename);
}

std::string comprehensive_file_manager::detect_file_format(const std::string& filename) {
    auto* parser = find_parser_for_file(filename);
    return parser ? parser->get_format_name() : "Unknown";
}

bool comprehensive_file_manager::convert_file(const std::string& input_filename, const std::string& output_filename, const std::string& target_format) {
    // Load input file
    music_data data;
    enhanced_music_metadata metadata;

    if (!load_file(input_filename, data, metadata)) {
        return false;
    }

    // Save to output format
    return save_file(output_filename, data, metadata);
}

enhanced_music_parser* comprehensive_file_manager::find_parser_for_file(const std::string& filename) {
    for (auto& parser : m_parsers) {
        if (parser->can_parse_file(filename)) {
            return parser.get();
        }
    }
    return nullptr;
}

bool comprehensive_file_manager::is_file_too_large(const std::string& filename) {
    size_t file_size = file_support_utils::get_file_size(filename);
    return file_size > (m_config.max_file_size_mb * 1024 * 1024);
}

std::string comprehensive_file_manager::generate_backup_filename(const std::string& original_filename) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << file_support_utils::remove_extension(original_filename)
        << "_backup_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
        << file_support_utils::get_file_extension(original_filename);
    return oss.str();
}

bool comprehensive_file_manager::is_metadata_cached(const std::string& filename, enhanced_music_metadata& metadata) {
    auto it = m_metadata_cache.find(filename);
    if (it != m_metadata_cache.end()) {
        // Check if file has been modified since cache
        auto current_mod_time = file_support_utils::get_file_modification_time(filename);
        if (it->second.modification_time == current_mod_time) {
            metadata = it->second;
            return true;
        } else {
            // File modified, remove from cache
            m_metadata_cache.erase(it);
        }
    }
    return false;
}

void comprehensive_file_manager::cache_metadata(const std::string& filename, const enhanced_music_metadata& metadata) {
    m_metadata_cache[filename] = metadata;
}

void comprehensive_file_manager::set_config(const manager_config& config) {
    m_config = config;
}

// File Support Utilities Implementation
namespace file_support_utils {

bool file_exists(const std::string& filename) {
    return std::filesystem::exists(filename);
}

bool is_file_readable(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

bool is_file_writable(const std::string& filename) {
    if (file_exists(filename)) {
        std::ofstream file(filename, std::ios::app);
        return file.good();
    } else {
        std::ofstream file(filename);
        bool writable = file.good();
        if (writable) {
            file.close();
            std::filesystem::remove(filename);
        }
        return writable;
    }
}

size_t get_file_size(const std::string& filename) {
    std::error_code ec;
    auto size = std::filesystem::file_size(filename, ec);
    return ec ? 0 : size;
}

std::chrono::system_clock::time_point get_file_modification_time(const std::string& filename) {
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(filename, ec);
    if (ec) {
        return std::chrono::system_clock::now();
    }

    // Convert file_time to system_clock time
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    return sctp;
}

std::string to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string get_file_extension(const std::string& filename) {
    std::filesystem::path path(filename);
    return path.extension().string();
}

std::string remove_extension(const std::string& filename) {
    std::filesystem::path path(filename);
    return path.stem().string();
}

std::string sanitize_filename(const std::string& filename) {
    std::string result = filename;
    std::string invalid_chars = "<>:\"/\\|?*";

    for (char& c : result) {
        if (invalid_chars.find(c) != std::string::npos) {
            c = '_';
        }
    }

    return result;
}

std::vector<uint8_t> get_channels_used(const music_data& data) {
    // TODO: Implement based on music_data interface
    return {};
}

std::pair<music_time_t, music_time_t> get_time_range(const music_data& data) {
    // TODO: Implement based on music_data interface
    return {0, 0};
}

uint32_t count_tempo_changes(const music_data& data) {
    // TODO: Implement based on music_data interface
    return 0;
}

bool is_note_in_nes_range(uint8_t note) {
    // NES APU can roughly play MIDI notes 21-108 (A0 to C8)
    return note >= 21 && note <= 108;
}

uint8_t suggest_nes_channel_for_note(uint8_t note, uint8_t velocity) {
    // Simple channel assignment heuristic
    if (note < 60) {
        return 2; // Triangle for bass
    } else if (velocity < 32) {
        return 3; // Noise for quiet percussive sounds
    } else {
        return (note % 2); // Alternate between pulse channels
    }
}

std::vector<std::string> analyze_nes_limitations(const music_data& data) {
    std::vector<std::string> limitations;
    // TODO: Implement comprehensive NES limitation analysis
    return limitations;
}

} // namespace file_support_utils

// Factory Implementation
std::unique_ptr<comprehensive_file_manager> comprehensive_file_support_factory::create_default_manager() {
    auto manager = std::make_unique<comprehensive_file_manager>();

    // Register all available parsers
    manager->register_parser(create_midi_parser());
    manager->register_parser(create_musicxml_parser());
    manager->register_parser(create_pattern_parser());

    return manager;
}

std::unique_ptr<comprehensive_file_manager> comprehensive_file_support_factory::create_nes_optimized_manager() {
    auto manager = create_default_manager();

    // Configure for NES-focused usage
    comprehensive_file_manager::manager_config config;
    config.enable_nes_analysis = true;
    config.enable_metadata_caching = true;
    config.strict_validation = false;
    config.max_file_size_mb = 50; // Smaller files for NES content
    config.enable_backup_on_conversion = true;

    manager->set_config(config);
    return manager;
}

std::unique_ptr<comprehensive_file_manager> comprehensive_file_support_factory::create_minimal_manager() {
    auto manager = std::make_unique<comprehensive_file_manager>();

    // Only register MIDI parser for minimal setup
    manager->register_parser(create_midi_parser());

    // Minimal configuration
    comprehensive_file_manager::manager_config config;
    config.enable_metadata_caching = false;
    config.enable_nes_analysis = false;
    config.strict_validation = true;
    config.max_file_size_mb = 10;
    config.enable_backup_on_conversion = false;

    manager->set_config(config);
    return manager;
}

std::unique_ptr<enhanced_midi_parser> comprehensive_file_support_factory::create_midi_parser() {
    return std::make_unique<enhanced_midi_parser>();
}

std::unique_ptr<enhanced_musicxml_parser> comprehensive_file_support_factory::create_musicxml_parser() {
    return std::make_unique<enhanced_musicxml_parser>();
}

std::unique_ptr<nes_pattern_parser> comprehensive_file_support_factory::create_pattern_parser() {
    return std::make_unique<nes_pattern_parser>();
}