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
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Write MIDI header
    file.write("MThd", 4);

    // Header length (6 bytes)
    uint32_t header_length = 6;
    write_big_endian_32(file, header_length);

    // Format type (1 = multiple tracks, synchronous)
    uint16_t format_type = 1;
    write_big_endian_16(file, format_type);

    // Count tracks needed (one per channel used + tempo track)
    std::set<uint8_t> channels_used;
    for (const auto& note : data.notes()) {
        channels_used.insert(note.channel);
    }
    uint16_t track_count = static_cast<uint16_t>(channels_used.size() + 1); // +1 for tempo track
    write_big_endian_16(file, track_count);

    // Ticks per quarter note
    uint16_t ticks_per_quarter = 480;
    write_big_endian_16(file, ticks_per_quarter);

    // Write tempo track
    write_tempo_track(file, data, metadata, ticks_per_quarter);

    // Write track for each channel
    for (uint8_t channel : channels_used) {
        write_channel_track(file, data, channel, ticks_per_quarter);
    }

    file.close();
    return file.good();
}

bool enhanced_midi_parser::optimize_for_nes(music_data& data, enhanced_music_metadata& metadata) {
    bool optimizations_applied = false;

    // Step 1: Limit to 5 NES channels (0-4: Pulse1, Pulse2, Triangle, Noise, DMC)
    std::set<uint8_t> used_channels;
    for (const auto& note : data.notes()) {
        used_channels.insert(note.channel);
    }

    if (used_channels.size() > 5) {
        // Map channels using intelligent assignment
        std::map<uint8_t, uint8_t> channel_mapping = create_intelligent_channel_mapping(data);

        // Apply channel mapping
        for (auto& note : data.notes()) {
            if (channel_mapping.find(note.channel) != channel_mapping.end()) {
                note.channel = channel_mapping[note.channel];
                optimizations_applied = true;
            }
        }

        // Update controls and programs too
        for (auto& control : data.controls()) {
            if (channel_mapping.find(control.channel) != channel_mapping.end()) {
                control.channel = channel_mapping[control.channel];
            }
        }
        for (auto& program : data.programs()) {
            if (channel_mapping.find(program.channel) != channel_mapping.end()) {
                program.channel = channel_mapping[program.channel];
            }
        }
    }

    // Step 2: Adjust note ranges for NES channels
    for (auto& note : data.notes()) {
        uint8_t original_note = note.note;

        switch (note.channel) {
            case 0: case 1: // Pulse channels: C1-B7 (24-95)
                if (note.note < 24) {
                    note.note = 24 + (note.note % 12); // Transpose up by octaves
                    optimizations_applied = true;
                } else if (note.note > 95) {
                    note.note = 95 - ((127 - note.note) % 12); // Transpose down by octaves
                    optimizations_applied = true;
                }
                break;

            case 2: // Triangle channel: A0-B6 (21-83)
                if (note.note < 21) {
                    note.note = 21 + (note.note % 12);
                    optimizations_applied = true;
                } else if (note.note > 83) {
                    note.note = 83 - ((127 - note.note) % 12);
                    optimizations_applied = true;
                }
                break;

            case 3: // Noise channel: map to percussion-like notes
                // Map to standard drum kit range (35-81)
                if (note.note < 35 || note.note > 81) {
                    note.note = 35 + (note.note % 47); // Wrap within drum range
                    optimizations_applied = true;
                }
                break;

            case 4: // DMC channel: limited sample range
                // Keep existing note but mark for sample mapping
                break;
        }
    }

    // Step 3: Optimize polyphony - NES can only play one note per channel
    for (uint8_t channel = 0; channel < 5; ++channel) {
        remove_overlapping_notes_for_channel(data, channel);
        optimizations_applied = true;
    }

    // Step 4: Adjust velocities for NES volume levels
    for (auto& note : data.notes()) {
        uint8_t original_velocity = note.velocity;

        // NES has 16 volume levels (0-15), map MIDI velocity (0-127) to these
        uint8_t nes_volume = (note.velocity * 15) / 127;
        note.velocity = (nes_volume * 127) / 15; // Convert back to MIDI range

        if (note.velocity != original_velocity) {
            optimizations_applied = true;
        }
    }

    // Update metadata with optimization info
    if (optimizations_applied) {
        metadata.nes_analysis.optimized_for_nes = true;
        metadata.nes_analysis.optimization_notes.push_back("Applied automatic NES optimization");
        metadata.nes_analysis.optimization_notes.push_back("Limited to 5 channels with intelligent assignment");
        metadata.nes_analysis.optimization_notes.push_back("Adjusted note ranges for NES hardware limits");
        metadata.nes_analysis.optimization_notes.push_back("Removed overlapping notes (monophonic channels)");
        metadata.nes_analysis.optimization_notes.push_back("Quantized velocities to NES volume levels");
    }

    return optimizations_applied;
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

void enhanced_music_parser::analyze_nes_compatibility(const music_data& data, enhanced_music_metadata& metadata) {
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

channel_analysis enhanced_music_parser::analyze_channel_musical_role(const music_data& data, uint8_t channel) {
    channel_analysis analysis;
    analysis.channel_id = channel;

    // Collect notes for this channel
    for (const auto& note : data.notes()) {
        if (note.channel == channel) {
            analysis.notes.push_back(note);
            analysis.note_count++;
            analysis.min_note = std::min(analysis.min_note, note.note);
            analysis.max_note = std::max(analysis.max_note, note.note);
            analysis.total_duration += note.duration;
        }
    }

    if (analysis.note_count > 0) {
        // Calculate average velocity and pitch
        uint32_t total_velocity = 0;
        uint32_t total_pitch = 0;
        for (const auto& note : analysis.notes) {
            total_velocity += note.velocity;
            total_pitch += note.note;
        }
        analysis.avg_velocity = total_velocity / analysis.note_count;
        analysis.average_pitch = static_cast<double>(total_pitch) / analysis.note_count;

        // Calculate note density (notes per time unit)
        if (analysis.total_duration > 0) {
            analysis.note_density = static_cast<double>(analysis.note_count) / analysis.total_duration * 1000.0;
        }

        // Calculate rhythmic complexity and regularity
        analysis.rhythmic_complexity = analysis.note_density * 0.1; // Simple approximation
        analysis.rhythm_regularity = 0.5; // Default value, could be enhanced with interval analysis

        // Detect percussion patterns
        analysis.is_percussion = detect_percussion_patterns(analysis);

        // Musical role detection heuristics
        if (channel == 9 || analysis.is_percussion) { // MIDI channel 10 (0-indexed as 9) is typically percussion
            analysis.is_percussion = true;
            analysis.suggested_nes_channel = 3; // Noise channel
        } else if (analysis.average_pitch < 50) { // Below D3
            analysis.is_bass = true;
            analysis.suggested_nes_channel = 2; // Triangle channel
        } else if (analysis.average_pitch >= 60) { // Above C4
            analysis.is_melody = true;
            analysis.suggested_nes_channel = (analysis.note_count > 50) ? 0 : 1; // Pulse channels
        } else {
            analysis.is_harmony = true;
            analysis.suggested_nes_channel = 1; // Pulse 2
        }
    }

    return analysis;
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

    std::map<uint8_t, channel_analysis> channel_data;

    // Analyze each original channel using the unified analysis function
    for (uint8_t channel : channels_used) {
        channel_data[channel] = analyze_channel_musical_role(data, channel);
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
        if (analysis.is_melody && pulse_channel < 2) {
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
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    // Set basic metadata
    metadata.filename = filename;
    metadata.file_format = "NES Pattern";
    metadata.file_size_bytes = file_support_utils::get_file_size(filename);
    metadata.modification_time = file_support_utils::get_file_modification_time(filename);

    try {
        // Parse JSON-based NES pattern format
        std::string json_content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());

        // Simple JSON parsing for NES pattern format
        return parse_nes_pattern_json(json_content, output, metadata);

    } catch (const std::exception& e) {
        // Fallback: Try to parse as simple text format
        file.clear();
        file.seekg(0);
        return parse_nes_pattern_text(file, output, metadata);
    }
}

file_validation_result nes_pattern_parser::validate_file(const std::string& filename) {
    file_validation_result result;
    result.format_detected = "NES Pattern";

    if (!file_support_utils::file_exists(filename)) {
        result.errors.push_back("File does not exist");
        return result;
    }

    // Validate file extension
    std::string ext = file_support_utils::to_lower(file_support_utils::get_file_extension(filename));
    if (ext != ".nesp" && ext != ".nespattern") {
        result.errors.push_back("Invalid file extension for NES pattern");
        result.is_valid = false;
        return result;
    }

    // Try to open and validate file content
    std::ifstream file(filename);
    if (!file.is_open()) {
        result.errors.push_back("Cannot open file");
        result.is_valid = false;
        return result;
    }

    // Check if it's valid JSON or text format
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

    if (content.empty()) {
        result.errors.push_back("File is empty");
        result.is_valid = false;
        return result;
    }

    // Validate JSON format
    if (content[0] == '{') {
        if (!validate_nes_pattern_json_structure(content, result.errors)) {
            result.is_valid = false;
            return result;
        }
    } else {
        // Validate text format
        if (!validate_nes_pattern_text_structure(content, result.errors)) {
            result.is_valid = false;
            return result;
        }
    }

    result.is_valid = true;
    return result;
}

bool nes_pattern_parser::can_parse_file(const std::string& filename) {
    std::string ext = file_support_utils::to_lower(file_support_utils::get_file_extension(filename));
    return ext == ".nesp" || ext == ".nespattern";
}

bool nes_pattern_parser::export_to_file(const music_data& data, const enhanced_music_metadata& metadata, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    // Export as JSON format
    file << "{\n";
    file << "  \"format\": \"NES_Pattern\",\n";
    file << "  \"version\": \"1.0\",\n";
    file << "  \"metadata\": {\n";
    file << "    \"title\": \"" << escape_json_string(metadata.title) << "\",\n";
    file << "    \"composer\": \"" << escape_json_string(metadata.composer) << "\",\n";
    file << "    \"tempo_bpm\": " << (metadata.tempo_bpm > 0 ? metadata.tempo_bpm : 120) << ",\n";
    file << "    \"ticks_per_quarter\": " << metadata.ticks_per_quarter << "\n";
    file << "  },\n";
    file << "  \"channels\": {\n";

    // Export each NES channel (0-4)
    bool first_channel = true;
    for (uint8_t channel = 0; channel < 5; ++channel) {
        std::vector<music_note> channel_notes;
        for (const auto& note : data.notes()) {
            if (note.channel == channel) {
                channel_notes.push_back(note);
            }
        }

        if (channel_notes.empty()) continue;

        if (!first_channel) file << ",\n";
        first_channel = false;

        const char* channel_names[] = {"pulse1", "pulse2", "triangle", "noise", "dmc"};
        file << "    \"" << channel_names[channel] << "\": {\n";
        file << "      \"type\": \"" << channel_names[channel] << "\",\n";
        file << "      \"notes\": [\n";

        // Sort notes by start time
        std::sort(channel_notes.begin(), channel_notes.end(),
                  [](const music_note& a, const music_note& b) {
                      return a.start < b.start;
                  });

        // Export notes
        for (size_t i = 0; i < channel_notes.size(); ++i) {
            const auto& note = channel_notes[i];
            if (i > 0) file << ",\n";

            file << "        {\n";
            file << "          \"time\": " << note.start << ",\n";
            file << "          \"note\": " << static_cast<int>(note.note) << ",\n";
            file << "          \"velocity\": " << static_cast<int>(note.velocity) << ",\n";
            file << "          \"duration\": " << note.duration << "\n";
            file << "        }";
        }

        file << "\n      ]\n";
        file << "    }";
    }

    file << "\n  }\n";
    file << "}\n";

    file.close();
    return file.good();
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

// MIDI export helper functions
void enhanced_midi_parser::write_big_endian_16(std::ofstream& file, uint16_t value) {
    file.put(static_cast<char>((value >> 8) & 0xFF));
    file.put(static_cast<char>(value & 0xFF));
}

void enhanced_midi_parser::write_big_endian_32(std::ofstream& file, uint32_t value) {
    file.put(static_cast<char>((value >> 24) & 0xFF));
    file.put(static_cast<char>((value >> 16) & 0xFF));
    file.put(static_cast<char>((value >> 8) & 0xFF));
    file.put(static_cast<char>(value & 0xFF));
}

void enhanced_midi_parser::write_variable_length(std::ofstream& file, uint32_t value) {
    uint32_t buffer = value & 0x7F;
    while ((value >>= 7)) {
        buffer <<= 8;
        buffer |= ((value & 0x7F) | 0x80);
    }
    while (true) {
        file.put(static_cast<char>(buffer & 0xFF));
        if (buffer & 0x80) {
            buffer >>= 8;
        } else {
            break;
        }
    }
}

void enhanced_midi_parser::write_tempo_track(std::ofstream& file, const music_data& data,
                                           const enhanced_music_metadata& metadata, uint16_t ticks_per_quarter) {
    // Start track chunk
    file.write("MTrk", 4);

    // We'll write the track length later
    std::streampos length_pos = file.tellp();
    write_big_endian_32(file, 0); // Placeholder for track length
    std::streampos track_start = file.tellp();

    // Track name
    write_variable_length(file, 0); // Delta time 0
    file.put(0xFF); // Meta event
    file.put(0x03); // Track name
    std::string track_name = "Tempo Track";
    write_variable_length(file, track_name.length());
    file.write(track_name.c_str(), track_name.length());

    // Set tempo (120 BPM default)
    write_variable_length(file, 0); // Delta time 0
    file.put(0xFF); // Meta event
    file.put(0x51); // Set tempo
    file.put(0x03); // Length: 3 bytes
    uint32_t microseconds_per_quarter = 500000; // 120 BPM
    file.put(static_cast<char>((microseconds_per_quarter >> 16) & 0xFF));
    file.put(static_cast<char>((microseconds_per_quarter >> 8) & 0xFF));
    file.put(static_cast<char>(microseconds_per_quarter & 0xFF));

    // End of track
    write_variable_length(file, 0); // Delta time 0
    file.put(0xFF); // Meta event
    file.put(0x2F); // End of track
    file.put(0x00); // Length: 0

    // Write actual track length
    std::streampos track_end = file.tellp();
    uint32_t track_length = static_cast<uint32_t>(track_end - track_start);
    file.seekp(length_pos);
    write_big_endian_32(file, track_length);
    file.seekp(track_end);
}

void enhanced_midi_parser::write_channel_track(std::ofstream& file, const music_data& data,
                                              uint8_t channel, uint16_t ticks_per_quarter) {
    // Start track chunk
    file.write("MTrk", 4);

    // We'll write the track length later
    std::streampos length_pos = file.tellp();
    write_big_endian_32(file, 0); // Placeholder for track length
    std::streampos track_start = file.tellp();

    // Track name
    write_variable_length(file, 0); // Delta time 0
    file.put(0xFF); // Meta event
    file.put(0x03); // Track name
    std::string track_name = "Channel " + std::to_string(channel + 1);
    write_variable_length(file, track_name.length());
    file.write(track_name.c_str(), track_name.length());

    // Collect and sort notes for this channel
    std::vector<std::pair<music_time_t, music_note>> channel_notes;
    for (const auto& note : data.notes()) {
        if (note.channel == channel) {
            channel_notes.emplace_back(note.start, note);
            // Add note off event
            music_note note_off = note;
            channel_notes.emplace_back(note.start + note.duration, note_off);
        }
    }

    // Sort by time
    std::sort(channel_notes.begin(), channel_notes.end());

    // Write MIDI events
    music_time_t last_time = 0;
    for (const auto& [event_time, note] : channel_notes) {
        uint32_t delta_time = static_cast<uint32_t>(event_time - last_time);
        write_variable_length(file, delta_time);

        bool is_note_off = (event_time == note.start + note.duration);
        uint8_t status = is_note_off ? (0x80 | channel) : (0x90 | channel);
        uint8_t velocity = is_note_off ? 0 : note.velocity;

        file.put(status);
        file.put(note.note);
        file.put(velocity);

        last_time = event_time;
    }

    // End of track
    write_variable_length(file, 0); // Delta time 0
    file.put(0xFF); // Meta event
    file.put(0x2F); // End of track
    file.put(0x00); // Length: 0

    // Write actual track length
    std::streampos track_end = file.tellp();
    uint32_t track_length = static_cast<uint32_t>(track_end - track_start);
    file.seekp(length_pos);
    write_big_endian_32(file, track_length);
    file.seekp(track_end);
}
// NES optimization helper functions  
std::map<uint8_t, uint8_t> enhanced_midi_parser::create_intelligent_channel_mapping(const music_data& data) {
    std::map<uint8_t, uint8_t> mapping;

    // Analyze each channel to determine its musical role
    std::map<uint8_t, channel_analysis> channel_roles;
    for (uint8_t channel = 0; channel < 16; ++channel) {
        channel_roles[channel] = analyze_channel_musical_role(data, channel);
    }

    // Sort channels by priority (bass -> melody -> harmony -> percussion)
    std::vector<std::pair<uint8_t, double>> channel_priorities;
    for (const auto& [channel, analysis] : channel_roles) {
        if (analysis.notes.empty()) continue;

        double priority = 0.0;
        if (analysis.is_bass) priority += 100.0;
        if (analysis.is_melody) priority += 50.0;
        if (analysis.is_percussion) priority += 25.0;
        priority += analysis.note_density * 10.0; // More active channels get higher priority

        channel_priorities.emplace_back(channel, priority);
    }

    // Sort by priority (highest first)
    std::sort(channel_priorities.begin(), channel_priorities.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Assign to NES channels based on musical role
    uint8_t nes_channel = 0;
    for (const auto& [original_channel, priority] : channel_priorities) {
        if (nes_channel >= 5) break; // Only 5 NES channels available

        const auto& analysis = channel_roles[original_channel];

        if (analysis.is_bass && nes_channel <= 2) {
            mapping[original_channel] = 2; // Triangle channel for bass
        } else if (analysis.is_percussion && nes_channel <= 3) {
            mapping[original_channel] = 3; // Noise channel for percussion
        } else if (analysis.is_melody && nes_channel <= 1) {
            mapping[original_channel] = nes_channel < 2 ? nes_channel : 0; // Pulse channels for melody
        } else {
            // Assign remaining channels sequentially
            if (nes_channel < 5) {
                mapping[original_channel] = nes_channel;
            }
        }
        nes_channel++;
    }

    return mapping;
}

void enhanced_midi_parser::remove_overlapping_notes_for_channel(music_data& data, uint8_t channel) {
    // Get all notes for this channel and sort by start time
    std::vector<std::reference_wrapper<music_note>> channel_notes;
    for (auto& note : data.notes()) {
        if (note.channel == channel) {
            channel_notes.push_back(std::ref(note));
        }
    }

    if (channel_notes.size() <= 1) return; // No overlaps possible

    // Sort by start time
    std::sort(channel_notes.begin(), channel_notes.end(),
              [](const music_note& a, const music_note& b) {
                  return a.start < b.start;
              });

    // Remove overlapping notes (keep the first note, truncate or remove later ones)  
    for (size_t i = 0; i < channel_notes.size() - 1; ++i) {
        music_note& current = channel_notes[i].get();
        music_note& next = channel_notes[i + 1].get();

        music_time_t current_end = current.start + current.duration;
        if (current_end > next.start) {
            // Truncate current note to avoid overlap
            current.duration = next.start - current.start;

            // If duration becomes too short, mark for removal
            if (current.duration < 10) { // Minimum 10 ticks
                current.duration = 0; // Mark for removal
            }
        }
    }

    // Remove notes marked for removal (duration = 0)
    auto& notes = const_cast<std::vector<music_note>&>(data.notes());
    notes.erase(std::remove_if(notes.begin(), notes.end(),
                               [channel](const music_note& note) {
                                   return note.channel == channel && note.duration == 0;
                               }), notes.end());
}

// Percussion pattern detection helper function
bool enhanced_music_parser::detect_percussion_patterns(const channel_analysis& analysis) {
    if (analysis.notes.empty()) return false;

    // Criteria for percussion detection:
    // 1. High rhythmic density (lots of short notes)
    // 2. Limited pitch variation (drums typically use specific notes)
    // 3. Short note durations (percussion is typically percussive)
    // 4. Notes in drum kit range (MIDI channel 10 or notes 35-81)

    // Check rhythmic density
    bool high_density = analysis.rhythmic_complexity > 0.5; // More than 0.5 notes per tick

    // Check pitch variation
    std::set<uint8_t> unique_pitches;
    music_time_t total_duration = 0;
    music_time_t short_note_count = 0;
    
    for (const auto& note : analysis.notes) {
        unique_pitches.insert(note.note);
        total_duration += note.duration;
        
        // Count notes shorter than quarter note (480 ticks at standard resolution)
        if (note.duration < 240) { // Eighth note or shorter
            short_note_count++;
        }
    }

    // Limited pitch variation suggests percussion
    bool limited_pitches = unique_pitches.size() <= 8; // Max 8 different percussion sounds

    // High percentage of short notes
    double short_note_ratio = static_cast<double>(short_note_count) / analysis.notes.size();
    bool mostly_short_notes = short_note_ratio > 0.6; // 60% or more short notes

    // Check if notes are in typical drum range (35-81 in MIDI)
    bool in_drum_range = true;
    for (uint8_t pitch : unique_pitches) {
        if (pitch < 35 || pitch > 81) {
            in_drum_range = false;
            break;
        }
    }

    // Check for repetitive patterns (common in percussion)
    bool has_patterns = false;
    if (analysis.notes.size() >= 4) {
        // Look for repeating rhythmic patterns
        std::vector<music_time_t> intervals;
        for (size_t i = 1; i < analysis.notes.size() && i < 16; ++i) {
            intervals.push_back(analysis.notes[i].start - analysis.notes[i-1].start);
        }
        
        // Check for repeated intervals (simple pattern detection)
        std::map<music_time_t, int> interval_counts;
        for (auto interval : intervals) {
            interval_counts[interval]++;
        }
        
        // If any interval appears more than twice, consider it a pattern
        for (const auto& [interval, count] : interval_counts) {
            if (count >= 3) {
                has_patterns = true;
                break;
            }
        }
    }

    // Combine criteria: need at least 2 of these conditions
    int percussion_score = 0;
    if (high_density) percussion_score++;
    if (limited_pitches) percussion_score++;
    if (mostly_short_notes) percussion_score++;
    if (in_drum_range) percussion_score++;
    if (has_patterns) percussion_score++;

    return percussion_score >= 2;
}
// NES pattern parsing helper functions
bool nes_pattern_parser::parse_nes_pattern_json(const std::string& json_content, music_data& output, enhanced_music_metadata& metadata) {
    // Simple JSON parsing - find key sections
    size_t channels_pos = json_content.find("\"channels\"");
    if (channels_pos == std::string::npos) {
        return false;
    }

    // Extract metadata
    extract_metadata_from_json(json_content, metadata);

    // Parse each channel
    const char* channel_names[] = {"pulse1", "pulse2", "triangle", "noise", "dmc"};
    for (uint8_t channel = 0; channel < 5; ++channel) {
        parse_channel_from_json(json_content, channel_names[channel], channel, output);
    }

    return true;
}

bool nes_pattern_parser::parse_nes_pattern_text(std::ifstream& file, music_data& output, enhanced_music_metadata& metadata) {
    std::string line;
    uint8_t current_channel = 0;

    metadata.title = "Text Pattern";
    metadata.composer = "Unknown";
    metadata.tempo_bpm = 120;

    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        // Channel declaration: CHANNEL 0, CHANNEL 1, etc.
        if (line.substr(0, 7) == "CHANNEL") {
            current_channel = static_cast<uint8_t>(std::stoi(line.substr(8)));
            continue;
        }

        // Parse note line: TIME NOTE VELOCITY DURATION
        std::istringstream iss(line);
        music_time_t time;
        int note, velocity;
        music_time_t duration;

        if (iss >> time >> note >> velocity >> duration) {
            music_note new_note;
            new_note.start = time;
            new_note.note = static_cast<uint8_t>(note);
            new_note.velocity = static_cast<uint8_t>(velocity);
            new_note.duration = duration;
            new_note.channel = current_channel;

            output.add_note(new_note);
        }
    }

    return true;
}

void nes_pattern_parser::extract_metadata_from_json(const std::string& json_content, enhanced_music_metadata& metadata) {
    // Simple JSON value extraction
    auto extract_string = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\": \"";
        size_t pos = json_content.find(search);
        if (pos != std::string::npos) {
            size_t start = pos + search.length();
            size_t end = json_content.find("\"", start);
            if (end != std::string::npos) {
                return json_content.substr(start, end - start);
            }
        }
        return "";
    };

    auto extract_number = [&](const std::string& key) -> int {
        std::string search = "\"" + key + "\": ";
        size_t pos = json_content.find(search);
        if (pos != std::string::npos) {
            size_t start = pos + search.length();
            size_t end = json_content.find_first_of(",}", start);
            if (end != std::string::npos) {
                return std::stoi(json_content.substr(start, end - start));
            }
        }
        return 0;
    };

    metadata.title = extract_string("title");
    metadata.composer = extract_string("composer");
    metadata.tempo_bpm = extract_number("tempo_bpm");
    metadata.ticks_per_quarter = extract_number("ticks_per_quarter");

    if (metadata.tempo_bpm == 0) metadata.tempo_bpm = 120;
    if (metadata.ticks_per_quarter == 0) metadata.ticks_per_quarter = 480;
}

void nes_pattern_parser::parse_channel_from_json(const std::string& json_content, const std::string& channel_name, uint8_t channel_id, music_data& output) {
    // Find channel section
    std::string search = "\"" + channel_name + "\"";
    size_t channel_pos = json_content.find(search);
    if (channel_pos == std::string::npos) return;

    // Find notes array
    size_t notes_pos = json_content.find("\"notes\"", channel_pos);
    if (notes_pos == std::string::npos) return;

    size_t array_start = json_content.find("[", notes_pos);
    size_t array_end = json_content.find("]", array_start);
    if (array_start == std::string::npos || array_end == std::string::npos) return;

    // Parse notes within the array
    std::string notes_section = json_content.substr(array_start + 1, array_end - array_start - 1);

    // Simple note parsing - look for note objects
    size_t pos = 0;
    while (pos < notes_section.length()) {
        size_t note_start = notes_section.find("{", pos);
        if (note_start == std::string::npos) break;

        size_t note_end = notes_section.find("}", note_start);
        if (note_end == std::string::npos) break;

        std::string note_json = notes_section.substr(note_start, note_end - note_start + 1);

        // Extract note values
        auto extract_value = [&](const std::string& key) -> int {
            std::string search_str = "\"" + key + "\": ";
            size_t key_pos = note_json.find(search_str);
            if (key_pos != std::string::npos) {
                size_t val_start = key_pos + search_str.length();
                size_t val_end = note_json.find_first_of(",}", val_start);
                if (val_end != std::string::npos) {
                    return std::stoi(note_json.substr(val_start, val_end - val_start));
                }
            }
            return 0;
        };

        music_note new_note;
        new_note.start = extract_value("time");
        new_note.note = static_cast<uint8_t>(extract_value("note"));
        new_note.velocity = static_cast<uint8_t>(extract_value("velocity"));
        new_note.duration = extract_value("duration");
        new_note.channel = channel_id;

        if (new_note.note > 0 && new_note.velocity > 0 && new_note.duration > 0) {
            output.add_note(new_note);
        }

        pos = note_end + 1;
    }
}

bool nes_pattern_parser::validate_nes_pattern_json_structure(const std::string& content, std::vector<std::string>& errors) {
    // Basic JSON structure validation
    if (content.empty()) {
        errors.push_back("Empty JSON content");
        return false;
    }

    if (content[0] != '{' || content.back() != '}') {
        errors.push_back("Invalid JSON structure - must be an object");
        return false;
    }

    // Check for required fields
    if (content.find("\"format\"") == std::string::npos) {
        errors.push_back("Missing required field: format");
        return false;
    }

    if (content.find("\"channels\"") == std::string::npos) {
        errors.push_back("Missing required field: channels");
        return false;
    }

    return true;
}

bool nes_pattern_parser::validate_nes_pattern_text_structure(const std::string& content, std::vector<std::string>& errors) {
    std::istringstream iss(content);
    std::string line;
    bool has_channel = false;
    bool has_notes = false;

    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line.substr(0, 7) == "CHANNEL") {
            has_channel = true;
            continue;
        }

        // Try to parse as note line
        std::istringstream line_iss(line);
        music_time_t time;
        int note, velocity;
        music_time_t duration;

        if (line_iss >> time >> note >> velocity >> duration) {
            has_notes = true;
        }
    }

    if (!has_channel) {
        errors.push_back("No channel declarations found");
        return false;
    }

    if (!has_notes) {
        errors.push_back("No valid note entries found");
        return false;
    }

    return true;
}

std::string nes_pattern_parser::escape_json_string(const std::string& str) {
    std::string escaped;
    for (char c : str) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c; break;
        }
    }
    return escaped;
}