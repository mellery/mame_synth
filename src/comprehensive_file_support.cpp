#include "comprehensive_file_support.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <set>
#include <map>
#include <limits>
#include <filesystem>
#include <chrono>

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
    // Use the core MusicXML parser for the actual parsing
    musicxml_parser core_parser;
    bool success = core_parser.parse_file(filename, output);

    if (!success) {
        return false;
    }

    // Extract enhanced metadata
    metadata.filename = filename;
    metadata.file_format = "MusicXML";
    metadata.file_size_bytes = file_support_utils::get_file_size(filename);
    metadata.modification_time = file_support_utils::get_file_modification_time(filename);

    // Copy basic metadata from core parser
    metadata.title = output.metadata().title;
    metadata.composer = output.metadata().composer;
    metadata.ticks_per_quarter = output.metadata().ticks_per_quarter;

    // Analyze music data for enhanced metadata
    metadata.track_count = 1; // Simplified for now
    metadata.total_notes = static_cast<uint32_t>(output.note_count());

    // Calculate timing information from notes
    music_time_t max_time = 0;
    if (!output.notes().empty()) {
        metadata.first_note_time = output.notes()[0].start;
        for (const auto& note : output.notes()) {
            music_time_t note_end = note.start + note.duration;
            if (note_end > max_time) {
                max_time = note_end;
            }
            if (note.start < metadata.first_note_time) {
                metadata.first_note_time = note.start;
            }
        }
    }

    metadata.last_note_time = max_time;
    metadata.total_ticks = max_time;

    // Extract additional MusicXML metadata from the XML file
    extract_musicxml_metadata(metadata);

    // Analyze NES compatibility
    analyze_nes_compatibility(output, metadata);

    return true;
}

file_validation_result enhanced_musicxml_parser::validate_file(const std::string& filename) {
    file_validation_result result;
    result.format_detected = "MusicXML";

    if (!file_support_utils::file_exists(filename)) {
        result.errors.push_back("File does not exist");
        return result;
    }

#ifdef HAVE_PUGIXML
    // Validate XML structure using pugixml
    pugi::xml_document doc;
    pugi::xml_parse_result parse_result = doc.load_file(filename.c_str());

    if (!parse_result) {
        result.errors.push_back("XML parsing error: " + std::string(parse_result.description()));
        result.errors.push_back("At offset: " + std::to_string(parse_result.offset));
        return result;
    }

    // Check for valid MusicXML root element
    pugi::xml_node root = doc.child("score-partwise");
    if (!root) {
        root = doc.child("score-timewise");
    }

    if (!root) {
        result.errors.push_back("Invalid MusicXML: missing score-partwise or score-timewise root element");
        return result;
    }

    // Check for required elements
    if (!root.child("part-list")) {
        result.warnings.push_back("Missing part-list element");
    }

    // Count parts
    int part_count = 0;
    for (pugi::xml_node part : root.children("part")) {
        part_count++;
    }

    if (part_count == 0) {
        result.warnings.push_back("No musical parts found");
    } else {
        result.info_messages.push_back("Found " + std::to_string(part_count) + " musical parts");
    }

    result.is_valid = true;
#else
    result.warnings.push_back("MusicXML validation limited - pugixml not available");
    result.is_valid = true; // Assume valid if we can't check properly
#endif

    return result;
}

bool enhanced_musicxml_parser::can_parse_file(const std::string& filename) {
    std::string ext = file_support_utils::to_lower(file_support_utils::get_file_extension(filename));
    return ext == ".xml" || ext == ".musicxml" || ext == ".mxl";
}

bool enhanced_musicxml_parser::optimize_for_nes(music_data& data, enhanced_music_metadata& metadata) {
    bool optimized = false;

    // Step 1: Analyze current music data
    auto [min_time, max_time] = file_support_utils::get_time_range(data);
    auto channels_used = file_support_utils::get_channels_used(data);

    // Step 2: Advanced channel assignment algorithm
    struct channel_analysis {
        std::vector<music_note> notes;
        uint8_t suggested_nes_channel;
        double average_pitch;
        double rhythmic_complexity;
        bool is_melodic;
        bool is_bass;
        bool is_percussive;
    };

    std::map<uint8_t, channel_analysis> channel_data;

    // Analyze each original channel
    for (const auto& note : data.notes()) {
        channel_data[note.channel].notes.push_back(note);
    }

    // Calculate analysis metrics for each channel
    for (auto& [channel, analysis] : channel_data) {
        if (analysis.notes.empty()) continue;

        // Calculate average pitch
        double total_pitch = 0;
        for (const auto& note : analysis.notes) {
            total_pitch += note.note;
        }
        analysis.average_pitch = total_pitch / analysis.notes.size();

        // Determine musical role
        analysis.is_bass = analysis.average_pitch < 50;       // Below D3
        analysis.is_melodic = analysis.average_pitch >= 60;   // Above C4
        analysis.is_percussive = false; // TODO: Detect percussion patterns

        // Calculate rhythmic complexity (note density)
        if (max_time > min_time) {
            double time_span = static_cast<double>(max_time - min_time);
            analysis.rhythmic_complexity = analysis.notes.size() / time_span;
        } else {
            analysis.rhythmic_complexity = 0;
        }
    }

    // Step 3: Intelligent NES channel assignment
    struct nes_channel_assignment {
        uint8_t original_channel;
        uint8_t nes_channel;
        std::string reason;
    };

    std::vector<nes_channel_assignment> assignments;
    std::vector<bool> nes_channels_used(5, false); // 5 NES channels

    // Priority assignment order:
    // 1. Bass parts -> Triangle channel (2)
    // 2. Melodic parts -> Pulse channels (0, 1)
    // 3. Percussive -> Noise channel (3)
    // 4. Remaining -> DMC or remaining pulse

    // Assign bass parts to triangle channel
    for (const auto& [channel, analysis] : channel_data) {
        if (analysis.is_bass && !nes_channels_used[2]) {
            assignments.push_back({channel, 2, "Bass part assigned to triangle channel"});
            nes_channels_used[2] = true;
            optimized = true;
        }
    }

    // Assign melodic parts to pulse channels
    uint8_t pulse_channel = 0;
    for (const auto& [channel, analysis] : channel_data) {
        if (analysis.is_melodic && pulse_channel < 2) {
            assignments.push_back({channel, pulse_channel,
                                 "Melodic part assigned to pulse channel " + std::to_string(pulse_channel)});
            nes_channels_used[pulse_channel] = true;
            pulse_channel++;
            optimized = true;
        }
    }

    // Assign remaining channels
    uint8_t next_available = 0;
    for (const auto& [channel, analysis] : channel_data) {
        // Skip if already assigned
        bool already_assigned = false;
        for (const auto& assignment : assignments) {
            if (assignment.original_channel == channel) {
                already_assigned = true;
                break;
            }
        }
        if (already_assigned) continue;

        // Find next available NES channel
        while (next_available < 5 && nes_channels_used[next_available]) {
            next_available++;
        }

        if (next_available < 5) {
            std::string reason = "Assigned to available NES channel " + std::to_string(next_available);
            assignments.push_back({channel, next_available, reason});
            nes_channels_used[next_available] = true;
            optimized = true;
        } else {
            // No more channels available - merge with existing
            assignments.push_back({channel, 0, "Merged with pulse channel 0 (channel limit exceeded)"});
            optimized = true;
        }
    }

    // Step 4: Apply channel reassignments and optimizations
    auto notes_copy = data.notes();
    auto programs_copy = data.programs();
    auto controls_copy = data.controls();
    auto tempos_copy = data.tempos();

    data.clear();

    // Create channel mapping
    std::map<uint8_t, uint8_t> channel_mapping;
    for (const auto& assignment : assignments) {
        channel_mapping[assignment.original_channel] = assignment.nes_channel;
        metadata.nes_analysis.optimization_suggestions.push_back(assignment.reason);
    }

    // Apply optimizations to notes
    for (auto note : notes_copy) {
        // Apply channel remapping
        if (channel_mapping.find(note.channel) != channel_mapping.end()) {
            note.channel = channel_mapping[note.channel];
        }

        // Channel-specific optimizations
        if (note.channel == 2) {
            // Triangle channel: No volume control, optimize for bass
            note.velocity = 127;
            if (note.note < 21) note.note = 21;
            if (note.note > 86) note.note = 86;
        } else if (note.channel == 3) {
            // Noise channel: Map pitches to noise periods
            note.note = map_pitch_to_noise_period(note.note);
        } else if (note.channel <= 1) {
            // Pulse channels: Full range but clamp extremes
            if (note.note < 24) note.note = 24;
            if (note.note > 107) note.note = 107;
        }

        // General NES range clamping
        if (note.note < 21) {
            note.note = 21;
            optimized = true;
        }
        if (note.note > 108) {
            note.note = 108;
            optimized = true;
        }

        data.add_note(note);
    }

    // Copy other events with channel remapping
    for (auto program : programs_copy) {
        if (channel_mapping.find(program.channel) != channel_mapping.end()) {
            program.channel = channel_mapping[program.channel];
        }
        data.add_program(program);
    }

    for (auto control : controls_copy) {
        if (channel_mapping.find(control.channel) != channel_mapping.end()) {
            control.channel = channel_mapping[control.channel];
        }
        // Note: Many controls may not work on NES, but we preserve them
        data.add_control(control);
    }

    for (const auto& tempo : tempos_copy) {
        data.add_tempo(tempo);
    }

    // Step 5: Update detailed NES analysis
    update_nes_analysis_detailed(data, metadata);

    if (optimized) {
        metadata.nes_analysis.optimization_suggestions.push_back(
            "Applied advanced NES optimization with intelligent channel assignment");
    }

    return optimized;
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
#ifdef HAVE_PUGIXML
    // Parse the file again to extract detailed metadata
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(metadata.filename.c_str());

    if (!result) {
        return; // Skip metadata extraction if file can't be parsed
    }

    pugi::xml_node root = doc.child("score-partwise");
    if (!root) {
        root = doc.child("score-timewise");
    }

    if (!root) {
        return;
    }

    // Extract work information
    pugi::xml_node work = root.child("work");
    if (work) {
        pugi::xml_node work_number = work.child("work-number");
        pugi::xml_node work_title = work.child("work-title");
        pugi::xml_node opus = work.child("opus");

        if (work_number && metadata.title.empty()) {
            metadata.title = work_number.text().as_string();
        }
        if (work_title) {
            if (metadata.title.empty()) {
                metadata.title = work_title.text().as_string();
            }
            metadata.album = work_title.text().as_string(); // Use work-title as album
        }
    }

    // Extract identification information
    pugi::xml_node identification = root.child("identification");
    if (identification) {
        // Extract creators
        for (pugi::xml_node creator : identification.children("creator")) {
            std::string type = creator.attribute("type").as_string();
            std::string name = creator.text().as_string();

            if (type == "composer") {
                metadata.composer = name;
            } else if (type == "lyricist") {
                metadata.arranger = name; // Use lyricist as arranger if no arranger
            } else if (type == "arranger") {
                metadata.arranger = name;
            }
        }

        // Extract rights (copyright)
        pugi::xml_node rights = identification.child("rights");
        if (rights) {
            metadata.copyright = rights.text().as_string();
        }

        // Extract encoding information for additional metadata
        pugi::xml_node encoding = identification.child("encoding");
        if (encoding) {
            pugi::xml_node software = encoding.child("software");
            if (software) {
                metadata.comments = "Created with: " + std::string(software.text().as_string());
            }
        }
    }

    // Extract defaults and attributes
    pugi::xml_node defaults = root.child("defaults");
    if (defaults) {
        // Could extract default tempo, key signature, etc.
    }

    // Set genre as classical by default for MusicXML
    if (metadata.genre.empty()) {
        metadata.genre = "Classical";
    }
#endif
}

void enhanced_musicxml_parser::analyze_nes_compatibility(const music_data& data, enhanced_music_metadata& metadata) {
    // Analyze how well the MusicXML data works with NES APU
    std::map<uint8_t, int> channel_counts;
    int min_note = 127, max_note = 0;

    // Analyze notes across all channels
    for (const auto& note : data.notes()) {
        channel_counts[note.channel]++;
        if (note.note < min_note) min_note = note.note;
        if (note.note > max_note) max_note = note.note;
    }

    metadata.nes_analysis.is_nes_compatible = channel_counts.size() <= 5;

    if (channel_counts.size() > 5) {
        metadata.nes_analysis.compatibility_warnings.push_back("More than 5 channels used - some notes may conflict");
    }

    // Check for out-of-range notes
    if (data.note_count() > 0 && (min_note < 21 || max_note > 108)) {
        metadata.nes_analysis.compatibility_warnings.push_back(
            "Notes outside typical NES range (A0-B7)"
        );
    }

    // Set usage percentages based on channel activity
    size_t channel_index = 0;
    for (const auto& [channel, count] : channel_counts) {
        if (channel_index >= 5) break;

        int usage_percent = std::min(100, (count * 100) / static_cast<int>(metadata.total_notes + 1));

        if (channel_index == 0) metadata.nes_analysis.pulse1_usage_percentage = usage_percent;
        else if (channel_index == 1) metadata.nes_analysis.pulse2_usage_percentage = usage_percent;
        else if (channel_index == 2) metadata.nes_analysis.triangle_usage_percentage = usage_percent;
        else if (channel_index == 3) metadata.nes_analysis.noise_usage_percentage = usage_percent;
        else if (channel_index == 4) metadata.nes_analysis.dmc_usage_percentage = usage_percent;

        channel_index++;
    }

    // Add optimization suggestions
    if (channel_counts.size() > 2) {
        metadata.nes_analysis.optimization_suggestions.push_back("Consider using pulse channels for melody");
    }
    if (channel_counts.size() > 3) {
        metadata.nes_analysis.optimization_suggestions.push_back("Use triangle channel for bass lines");
    }
}

void enhanced_musicxml_parser::update_nes_analysis_detailed(const music_data& data, enhanced_music_metadata& metadata) {
    // Reset analysis
    metadata.nes_analysis.pulse1_usage_percentage = 0;
    metadata.nes_analysis.pulse2_usage_percentage = 0;
    metadata.nes_analysis.triangle_usage_percentage = 0;
    metadata.nes_analysis.noise_usage_percentage = 0;
    metadata.nes_analysis.dmc_usage_percentage = 0;

    // Count notes per NES channel
    std::map<uint8_t, int> channel_note_counts;
    for (const auto& note : data.notes()) {
        if (note.channel < 5) {
            channel_note_counts[note.channel]++;
        }
    }

    // Calculate usage percentages
    int total_notes = static_cast<int>(data.note_count());
    if (total_notes > 0) {
        metadata.nes_analysis.pulse1_usage_percentage =
            static_cast<uint8_t>((channel_note_counts[0] * 100) / total_notes);
        metadata.nes_analysis.pulse2_usage_percentage =
            static_cast<uint8_t>((channel_note_counts[1] * 100) / total_notes);
        metadata.nes_analysis.triangle_usage_percentage =
            static_cast<uint8_t>((channel_note_counts[2] * 100) / total_notes);
        metadata.nes_analysis.noise_usage_percentage =
            static_cast<uint8_t>((channel_note_counts[3] * 100) / total_notes);
        metadata.nes_analysis.dmc_usage_percentage =
            static_cast<uint8_t>((channel_note_counts[4] * 100) / total_notes);
    }

    // Determine overall compatibility
    auto channels_used = file_support_utils::get_channels_used(data);
    metadata.nes_analysis.is_nes_compatible = channels_used.size() <= 5;

    // Add detailed compatibility warnings
    auto limitations = file_support_utils::analyze_nes_limitations(data);
    for (const auto& limitation : limitations) {
        if (limitation != "No significant NES limitations detected") {
            metadata.nes_analysis.compatibility_warnings.push_back(limitation);
        }
    }

    // Add optimization suggestions based on analysis
    if (metadata.nes_analysis.pulse1_usage_percentage == 0 &&
        metadata.nes_analysis.pulse2_usage_percentage == 0) {
        metadata.nes_analysis.optimization_suggestions.push_back(
            "Consider using pulse channels for melodic content");
    }

    if (metadata.nes_analysis.triangle_usage_percentage == 0 &&
        channel_note_counts[0] + channel_note_counts[1] > 0) {
        metadata.nes_analysis.optimization_suggestions.push_back(
            "Consider using triangle channel for bass lines");
    }

    if (channels_used.size() > 5) {
        metadata.nes_analysis.optimization_suggestions.push_back(
            "Reduce to 5 channels or fewer for full NES compatibility");
    }
}

uint8_t enhanced_musicxml_parser::map_pitch_to_noise_period(uint8_t pitch) {
    // NES noise channel has 16 different periods (0-15)
    // Map MIDI pitch range to these periods
    // Higher pitches = lower periods (higher frequencies)

    if (pitch >= 96) return 0;  // Very high pitch -> period 0 (highest freq)
    else if (pitch >= 84) return 1;
    else if (pitch >= 72) return 2;
    else if (pitch >= 66) return 3;
    else if (pitch >= 60) return 4;  // Middle C
    else if (pitch >= 54) return 5;
    else if (pitch >= 48) return 6;
    else if (pitch >= 42) return 7;
    else if (pitch >= 36) return 8;
    else if (pitch >= 30) return 9;
    else if (pitch >= 24) return 10;
    else if (pitch >= 18) return 11;
    else if (pitch >= 12) return 12;
    else if (pitch >= 6) return 13;
    else if (pitch >= 1) return 14;
    else return 15;  // Very low pitch -> period 15 (lowest freq)
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
    std::set<uint8_t> channels_set;

    // Collect channels from notes
    for (const auto& note : data.notes()) {
        channels_set.insert(note.channel);
    }

    // Collect channels from controls
    for (const auto& control : data.controls()) {
        channels_set.insert(control.channel);
    }

    // Collect channels from programs
    for (const auto& program : data.programs()) {
        channels_set.insert(program.channel);
    }

    return std::vector<uint8_t>(channels_set.begin(), channels_set.end());
}

std::pair<music_time_t, music_time_t> get_time_range(const music_data& data) {
    if (data.empty()) {
        return {0, 0};
    }

    music_time_t min_time = std::numeric_limits<music_time_t>::max();
    music_time_t max_time = 0;

    // Check notes
    for (const auto& note : data.notes()) {
        min_time = std::min(min_time, note.start);
        max_time = std::max(max_time, note.start + note.duration);
    }

    // Check controls
    for (const auto& control : data.controls()) {
        min_time = std::min(min_time, control.time);
        max_time = std::max(max_time, control.time);
    }

    // Check programs
    for (const auto& program : data.programs()) {
        min_time = std::min(min_time, program.time);
        max_time = std::max(max_time, program.time);
    }

    // Check tempos
    for (const auto& tempo : data.tempos()) {
        min_time = std::min(min_time, tempo.time);
        max_time = std::max(max_time, tempo.time);
    }

    // Handle case where no events found
    if (min_time == std::numeric_limits<music_time_t>::max()) {
        min_time = 0;
    }

    return {min_time, max_time};
}

uint32_t count_tempo_changes(const music_data& data) {
    return static_cast<uint32_t>(data.tempos().size());
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

    // Check total number of channels
    auto channels_used = get_channels_used(data);
    if (channels_used.size() > 5) {
        limitations.push_back("Uses " + std::to_string(channels_used.size()) +
                            " channels (NES APU has only 5 channels)");
    }

    // Check note range compatibility
    int out_of_range_notes = 0;
    uint8_t lowest_note = 127, highest_note = 0;

    for (const auto& note : data.notes()) {
        if (!is_note_in_nes_range(note.note)) {
            out_of_range_notes++;
        }
        if (!data.notes().empty()) {
            lowest_note = std::min(lowest_note, note.note);
            highest_note = std::max(highest_note, note.note);
        }
    }

    if (out_of_range_notes > 0) {
        limitations.push_back(std::to_string(out_of_range_notes) +
                            " notes outside NES frequency range");
    }

    // Check for polyphony per channel
    std::map<uint8_t, std::vector<music_time_t>> channel_note_times;
    for (const auto& note : data.notes()) {
        channel_note_times[note.channel].push_back(note.start);
    }

    for (const auto& [channel, times] : channel_note_times) {
        // Check for overlapping notes on same channel
        auto sorted_times = times;
        std::sort(sorted_times.begin(), sorted_times.end());

        int max_simultaneous = 1;
        int current_simultaneous = 1;

        for (size_t i = 1; i < sorted_times.size(); ++i) {
            if (sorted_times[i] == sorted_times[i-1]) {
                current_simultaneous++;
                max_simultaneous = std::max(max_simultaneous, current_simultaneous);
            } else {
                current_simultaneous = 1;
            }
        }

        if (max_simultaneous > 1) {
            limitations.push_back("Channel " + std::to_string(channel) +
                                " has " + std::to_string(max_simultaneous) +
                                " simultaneous notes (NES channels are monophonic)");
        }
    }

    // Check for excessive tempo changes
    if (data.tempos().size() > 10) {
        limitations.push_back("High number of tempo changes (" +
                            std::to_string(data.tempos().size()) +
                            ") may impact NES performance");
    }

    // Check for control changes that NES can't handle
    if (!data.controls().empty()) {
        limitations.push_back("Contains " + std::to_string(data.controls().size()) +
                            " control changes (NES APU has limited control capabilities)");
    }

    if (limitations.empty()) {
        limitations.push_back("No significant NES limitations detected");
    }

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