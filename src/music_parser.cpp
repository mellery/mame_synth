#include "music_parser.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>

// MIDI Parser Implementation
midi_parser::midi_parser() = default;

bool midi_parser::parse_file(const std::string& filename, music_data& data) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        set_error("Cannot open file: " + filename);
        return false;
    }

    // Read entire file into buffer
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(file_size);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);

    if (!file.good() && !file.eof()) {
        set_error("Error reading file: " + filename);
        return false;
    }

    return parse_buffer(buffer, data);
}

bool midi_parser::parse_buffer(const std::vector<uint8_t>& buffer, music_data& data) {
    if (buffer.empty()) {
        set_error("Empty buffer provided");
        return false;
    }

    return parse_midi_buffer(buffer.data(), buffer.size(), data);
}

bool midi_parser::supports_extension(const std::string& ext) const {
    std::string lower_ext = ext;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(), ::tolower);
    return lower_ext == ".mid" || lower_ext == ".midi";
}

bool midi_parser::parse_midi_buffer(const uint8_t* data, size_t size, music_data& output) {
    if (size < 14) {
        set_error("File too small to be valid MIDI");
        return false;
    }

    // Check MIDI header signature "MThd"
    if (data[0] != 'M' || data[1] != 'T' || data[2] != 'h' || data[3] != 'd') {
        set_error("Invalid MIDI file header");
        return false;
    }

    // Parse header
    if (!parse_header_chunk(data, size, output.metadata())) {
        return false;
    }

    // Move to track data
    const uint8_t* track_data = data + 14; // Skip header chunk
    size_t remaining = size - 14;

    // Parse each track
    for (uint16_t track = 0; track < output.metadata().track_count && remaining > 8; ++track) {
        // Check track header "MTrk"
        if (track_data[0] != 'M' || track_data[1] != 'T' ||
            track_data[2] != 'r' || track_data[3] != 'k') {
            set_error("Invalid track header in track " + std::to_string(track));
            return false;
        }

        uint32_t track_length = read_uint32_be(track_data + 4);
        if (remaining < 8 + track_length) {
            set_error("Track length exceeds file size");
            return false;
        }

        if (!parse_track_chunk(track_data + 8, track_length, output)) {
            return false;
        }

        track_data += 8 + track_length;
        remaining -= 8 + track_length;
    }

    std::cout << "MIDI parsing completed: " << output.note_count() << " notes, "
              << output.event_count() << " total events" << std::endl;

    return true;
}

bool midi_parser::parse_header_chunk(const uint8_t* data, size_t size, music_metadata& meta) {
    if (size < 14) return false;

    uint32_t header_length = read_uint32_be(data + 4);
    if (header_length != 6) {
        set_error("Invalid MIDI header length");
        return false;
    }

    uint16_t format_type = read_uint16_be(data + 8);
    uint16_t track_count = read_uint16_be(data + 10);
    uint16_t time_division = read_uint16_be(data + 12);

    // Only support format 0 and 1 for now
    if (format_type > 1) {
        set_error("Unsupported MIDI format type: " + std::to_string(format_type));
        return false;
    }

    meta.track_count = track_count;

    // Handle time division
    if (time_division & 0x8000) {
        // SMPTE time format (not commonly used)
        set_error("SMPTE time format not supported");
        return false;
    } else {
        // Ticks per quarter note
        meta.ticks_per_quarter = time_division;
    }

    return true;
}

bool midi_parser::parse_track_chunk(const uint8_t* data, size_t size, music_data& output) {
    const uint8_t* current = data;
    size_t remaining = size;
    music_time_t current_time = 0;
    uint8_t running_status = 0;

    while (remaining > 0) {
        // Read delta time
        uint32_t delta_time = read_variable_length(current, remaining);
        current_time += delta_time;

        if (remaining == 0) break;

        uint8_t status_byte = *current;

        // Handle running status
        if (status_byte & 0x80) {
            running_status = status_byte;
            ++current;
            --remaining;
        } else {
            status_byte = running_status;
        }

        if (remaining == 0) break;

        // Parse different event types
        if ((status_byte & 0xF0) == 0x90) {
            // Note On
            if (remaining < 2) break;
            uint8_t channel = status_byte & 0x0F;
            uint8_t note = current[0];
            uint8_t velocity = current[1];

            if (velocity > 0) {
                // TODO: Handle note duration properly by tracking note off events
                // For now, use a default duration
                music_note music_note(channel, note, velocity, current_time, 480);
                output.add_note(music_note);
            }

            current += 2;
            remaining -= 2;
        }
        else if ((status_byte & 0xF0) == 0x80) {
            // Note Off - skip for now, duration handling will be improved later
            if (remaining < 2) break;
            current += 2;
            remaining -= 2;
        }
        else if ((status_byte & 0xF0) == 0xB0) {
            // Control Change
            if (remaining < 2) break;
            uint8_t channel = status_byte & 0x0F;
            uint8_t controller = current[0];
            uint8_t value = current[1];

            music_control ctrl(channel, controller, value, current_time);
            output.add_control(ctrl);

            current += 2;
            remaining -= 2;
        }
        else if ((status_byte & 0xF0) == 0xC0) {
            // Program Change
            if (remaining < 1) break;
            uint8_t channel = status_byte & 0x0F;
            uint8_t program = current[0];

            music_program prog(channel, program, current_time);
            output.add_program(prog);

            current += 1;
            remaining -= 1;
        }
        else if (status_byte == 0xFF) {
            // Meta Event
            if (remaining < 2) break;
            uint8_t meta_type = current[0];
            ++current;
            --remaining;

            uint32_t meta_length = read_variable_length(current, remaining);
            if (remaining < meta_length) break;

            if (meta_type == 0x51 && meta_length == 3) {
                // Tempo change
                uint32_t tempo = (current[0] << 16) | (current[1] << 8) | current[2];
                music_tempo tempo_event(tempo, current_time);
                output.add_tempo(tempo_event);
            }
            // TODO: Handle other meta events (track name, copyright, etc.)

            current += meta_length;
            remaining -= meta_length;
        }
        else {
            // Skip unknown events
            ++current;
            --remaining;
        }
    }

    return true;
}

uint32_t midi_parser::read_variable_length(const uint8_t*& data, size_t& remaining) {
    uint32_t value = 0;
    uint8_t byte;

    do {
        if (remaining == 0) return value;
        byte = *data;
        ++data;
        --remaining;

        value = (value << 7) | (byte & 0x7F);
    } while (byte & 0x80);

    return value;
}

uint32_t midi_parser::read_uint32_be(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

uint16_t midi_parser::read_uint16_be(const uint8_t* data) {
    return (data[0] << 8) | data[1];
}

// MusicXML Parser Stub Implementation
musicxml_parser::musicxml_parser() = default;

bool musicxml_parser::parse_file(const std::string& filename, music_data& data) {
    set_error("MusicXML parsing not yet implemented");
    // TODO: Implement MusicXML parsing
    // This is a placeholder for future development
    std::cout << "MusicXML parser called for: " << filename << std::endl;
    return false;
}

bool musicxml_parser::parse_buffer(const std::vector<uint8_t>& buffer, music_data& data) {
    set_error("MusicXML parsing not yet implemented");
    // TODO: Implement MusicXML parsing from buffer
    std::cout << "MusicXML parser called with buffer size: " << buffer.size() << std::endl;
    return false;
}

bool musicxml_parser::supports_extension(const std::string& ext) const {
    std::string lower_ext = ext;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(), ::tolower);
    return lower_ext == ".xml" || lower_ext == ".musicxml";
}

// Parser Factory Implementation
std::unique_ptr<music_parser> music_parser_factory::create_parser(const std::string& filename) {
    // Extract extension
    size_t dot_pos = filename.rfind('.');
    if (dot_pos == std::string::npos) {
        return nullptr;
    }

    std::string ext = filename.substr(dot_pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".mid" || ext == ".midi") {
        return std::make_unique<midi_parser>();
    }
    else if (ext == ".xml" || ext == ".musicxml") {
        return std::make_unique<musicxml_parser>();
    }

    return nullptr;
}

std::vector<std::string> music_parser_factory::supported_extensions() {
    return {".mid", ".midi", ".xml", ".musicxml"};
}

bool music_parser_factory::is_supported(const std::string& filename) {
    return create_parser(filename) != nullptr;
}