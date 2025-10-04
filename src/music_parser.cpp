#include "music_parser.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <cmath>
#include <map>

#ifdef HAVE_PUGIXML
#include <pugixml.hpp>
#endif

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

    // Track active notes: key = (channel << 8) | note, value = {start_time, velocity}
    std::map<uint16_t, std::pair<music_time_t, uint8_t>> active_notes;

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

            uint16_t note_key = (channel << 8) | note;

            if (velocity > 0) {
                // Store note start time and velocity
                active_notes[note_key] = {current_time, velocity};
            } else {
                // Velocity 0 is equivalent to Note Off
                auto it = active_notes.find(note_key);
                if (it != active_notes.end()) {
                    music_time_t start_time = it->second.first;
                    uint8_t note_velocity = it->second.second;
                    music_time_t duration = current_time - start_time;

                    music_note music_note(channel, note, note_velocity, start_time, duration);
                    output.add_note(music_note);
                    active_notes.erase(it);
                }
            }

            current += 2;
            remaining -= 2;
        }
        else if ((status_byte & 0xF0) == 0x80) {
            // Note Off
            if (remaining < 2) break;
            uint8_t channel = status_byte & 0x0F;
            uint8_t note = current[0];
            // uint8_t velocity = current[1];  // Note off velocity (usually ignored)

            uint16_t note_key = (channel << 8) | note;
            auto it = active_notes.find(note_key);
            if (it != active_notes.end()) {
                music_time_t start_time = it->second.first;
                uint8_t note_velocity = it->second.second;
                music_time_t duration = current_time - start_time;

                music_note music_note(channel, note, note_velocity, start_time, duration);
                output.add_note(music_note);
                active_notes.erase(it);
            }

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

// MusicXML Parser Implementation
musicxml_parser::musicxml_parser() = default;

bool musicxml_parser::parse_file(const std::string& filename, music_data& data) {
#ifdef HAVE_PUGIXML
    try {
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file(filename.c_str());

        if (!result) {
            set_error("Failed to parse XML file: " + std::string(result.description()));
            return false;
        }

        return parse_musicxml_document(doc, data);
    } catch (const std::exception& e) {
        set_error("Exception parsing MusicXML file: " + std::string(e.what()));
        return false;
    }
#else
    set_error("MusicXML support not available - pugixml library not found");
    return false;
#endif
}

bool musicxml_parser::parse_buffer(const std::vector<uint8_t>& buffer, music_data& data) {
#ifdef HAVE_PUGIXML
    try {
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_buffer(buffer.data(), buffer.size());

        if (!result) {
            set_error("Failed to parse XML buffer: " + std::string(result.description()));
            return false;
        }

        return parse_musicxml_document(doc, data);
    } catch (const std::exception& e) {
        set_error("Exception parsing MusicXML buffer: " + std::string(e.what()));
        return false;
    }
#else
    set_error("MusicXML support not available - pugixml library not found");
    return false;
#endif
}

bool musicxml_parser::supports_extension(const std::string& ext) const {
    std::string lower_ext = ext;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(), ::tolower);
    return lower_ext == ".xml" || lower_ext == ".musicxml";
}

#ifdef HAVE_PUGIXML
bool musicxml_parser::parse_musicxml_document(const pugi::xml_document& doc, music_data& data) {
    // Find the root element - could be <score-partwise> or <score-timewise>
    pugi::xml_node root = doc.child("score-partwise");
    bool is_partwise = true;

    if (!root) {
        root = doc.child("score-timewise");
        is_partwise = false;
    }

    if (!root) {
        set_error("Invalid MusicXML: missing score-partwise or score-timewise root element");
        return false;
    }

    // Initialize music data
    data.clear();
    data.metadata().ticks_per_quarter = 480; // Default MIDI-compatible value

    // Parse work and movement information
    parse_work_info(root, data.metadata());

    // Parse part list to understand instruments
    std::vector<part_info> parts;
    if (!parse_part_list(root.child("part-list"), parts)) {
        set_error("Failed to parse part-list");
        return false;
    }

    // Parse musical content
    if (is_partwise) {
        return parse_partwise_score(root, parts, data);
    } else {
        return parse_timewise_score(root, parts, data);
    }
}

void musicxml_parser::parse_work_info(const pugi::xml_node& root, music_metadata& metadata) {
    // Parse work information
    pugi::xml_node work = root.child("work");
    if (work) {
        pugi::xml_node work_title = work.child("work-title");
        if (work_title) {
            metadata.title = work_title.text().as_string();
        }
    }

    // Parse movement information
    pugi::xml_node movement_title = root.child("movement-title");
    if (movement_title) {
        if (metadata.title.empty()) {
            metadata.title = movement_title.text().as_string();
        }
    }

    // Parse identification for composer, etc.
    pugi::xml_node identification = root.child("identification");
    if (identification) {
        for (pugi::xml_node creator : identification.children("creator")) {
            std::string type = creator.attribute("type").as_string();
            std::string name = creator.text().as_string();

            if (type == "composer" && metadata.composer.empty()) {
                metadata.composer = name;
            }
        }
    }
}

bool musicxml_parser::parse_part_list(const pugi::xml_node& part_list, std::vector<part_info>& parts) {
    if (!part_list) {
        set_error("Missing part-list element");
        return false;
    }

    for (pugi::xml_node score_part : part_list.children("score-part")) {
        part_info part;
        part.id = score_part.attribute("id").as_string();

        pugi::xml_node part_name = score_part.child("part-name");
        if (part_name) {
            part.name = part_name.text().as_string();
        }

        // Parse MIDI instrument information
        pugi::xml_node midi_instrument = score_part.child("midi-instrument");
        if (midi_instrument) {
            pugi::xml_node midi_channel = midi_instrument.child("midi-channel");
            pugi::xml_node midi_program = midi_instrument.child("midi-program");

            if (midi_channel) {
                part.midi_channel = midi_channel.text().as_int() - 1; // Convert to 0-based
            }
            if (midi_program) {
                part.midi_program = midi_program.text().as_int() - 1; // Convert to 0-based
            }
        }

        parts.push_back(part);
    }

    return !parts.empty();
}

bool musicxml_parser::parse_partwise_score(const pugi::xml_node& root, const std::vector<part_info>& parts, music_data& data) {
    // Parse each part
    size_t part_index = 0;
    for (pugi::xml_node part : root.children("part")) {
        std::string part_id = part.attribute("id").as_string();

        // Find corresponding part info
        auto part_it = std::find_if(parts.begin(), parts.end(),
            [&part_id](const part_info& p) { return p.id == part_id; });

        if (part_it == parts.end()) {
            continue; // Skip unknown parts
        }

        uint8_t channel = static_cast<uint8_t>(part_it->midi_channel);
        uint8_t program = static_cast<uint8_t>(part_it->midi_program);

        // Add program change if specified
        if (program > 0) {
            data.add_program(music_program(channel, program, 0));
        }

        // Parse measures in this part
        music_time_t current_time = 0;
        int current_divisions = 1;

        for (pugi::xml_node measure : part.children("measure")) {
            current_time = parse_measure(measure, channel, current_time, current_divisions, data);
        }

        part_index++;
    }

    // Update metadata
    data.metadata().track_count = static_cast<uint16_t>(parts.size());

    return true;
}

bool musicxml_parser::parse_timewise_score(const pugi::xml_node& root, const std::vector<part_info>& parts, music_data& data) {
    // Add program changes for each part
    for (size_t i = 0; i < parts.size(); ++i) {
        uint8_t channel = static_cast<uint8_t>(parts[i].midi_channel);
        uint8_t program = static_cast<uint8_t>(parts[i].midi_program);

        if (program > 0) {
            data.add_program(music_program(channel, program, 0));
        }
    }

    // Parse measures (time-wise organization)
    music_time_t current_time = 0;
    int current_divisions = 1;

    for (pugi::xml_node measure : root.children("measure")) {
        // Each measure contains parts
        size_t part_index = 0;
        for (pugi::xml_node part : measure.children("part")) {
            if (part_index >= parts.size()) {
                break;
            }

            uint8_t channel = static_cast<uint8_t>(parts[part_index].midi_channel);
            parse_measure_content(part, channel, current_time, current_divisions, data);
            part_index++;
        }

        // Advance time by measure duration
        current_time += calculate_measure_duration(measure, current_divisions);
    }

    // Update metadata
    data.metadata().track_count = static_cast<uint16_t>(parts.size());

    return true;
}

music_time_t musicxml_parser::parse_measure(const pugi::xml_node& measure, uint8_t channel,
                                          music_time_t start_time, int& divisions, music_data& data) {
    music_time_t current_time = start_time;

    // Parse attributes first (they might change divisions)
    for (pugi::xml_node attributes : measure.children("attributes")) {
        pugi::xml_node divisions_node = attributes.child("divisions");
        if (divisions_node) {
            divisions = divisions_node.text().as_int();
        }
    }

    // Parse musical content
    for (pugi::xml_node child : measure.children()) {
        std::string name = child.name();

        if (name == "note") {
            current_time += parse_note(child, channel, current_time, divisions, data);
        } else if (name == "backup") {
            pugi::xml_node duration = child.child("duration");
            if (duration) {
                current_time -= duration.text().as_int() * (480 / divisions); // Use constant for now
            }
        } else if (name == "forward") {
            pugi::xml_node duration = child.child("duration");
            if (duration) {
                current_time += duration.text().as_int() * (480 / divisions); // Use constant for now
            }
        }
    }

    return current_time;
}

music_time_t musicxml_parser::parse_note(const pugi::xml_node& note, uint8_t channel,
                                        music_time_t start_time, int divisions, music_data& data) {
    // Check if this is a rest
    if (note.child("rest")) {
        pugi::xml_node duration = note.child("duration");
        if (duration) {
            return duration.text().as_int() * (480 / divisions); // Use constant for now
        }
        return 0;
    }

    // Parse pitch
    pugi::xml_node pitch = note.child("pitch");
    if (!pitch) {
        return 0; // Skip unpitched notes for now
    }

    std::string step = pitch.child("step").text().as_string();
    int octave = pitch.child("octave").text().as_int();
    int alter = 0;

    pugi::xml_node alter_node = pitch.child("alter");
    if (alter_node) {
        alter = alter_node.text().as_int();
    }

    // Convert to MIDI note number
    uint8_t midi_note = convert_pitch_to_midi(step, octave, alter);

    // Parse duration
    pugi::xml_node duration = note.child("duration");
    music_time_t note_duration = 0;
    if (duration) {
        note_duration = duration.text().as_int() * (480 / divisions); // Use constant for now
    }

    // Parse velocity (dynamics)
    uint8_t velocity = 64; // Default
    pugi::xml_node notations = note.child("notations");
    if (notations) {
        // Look for dynamics
        for (pugi::xml_node dynamics : notations.children("dynamics")) {
            velocity = parse_dynamics(dynamics);
            break;
        }
    }

    // Add the note to music data (combining note on/off into single note)
    if (note_duration > 0) {
        music_note music_note_event(channel, midi_note, velocity, start_time, note_duration);
        data.add_note(music_note_event);
    }

    return note_duration;
}

uint8_t musicxml_parser::convert_pitch_to_midi(const std::string& step, int octave, int alter) {
    // Convert step to semitone within octave
    int semitone = 0;
    if (step == "C") semitone = 0;
    else if (step == "D") semitone = 2;
    else if (step == "E") semitone = 4;
    else if (step == "F") semitone = 5;
    else if (step == "G") semitone = 7;
    else if (step == "A") semitone = 9;
    else if (step == "B") semitone = 11;

    // Apply alteration
    semitone += alter;

    // Calculate MIDI note number (C4 = 60)
    int midi_note = (octave + 1) * 12 + semitone;

    // Clamp to valid MIDI range
    if (midi_note < 0) midi_note = 0;
    if (midi_note > 127) midi_note = 127;

    return static_cast<uint8_t>(midi_note);
}

uint8_t musicxml_parser::parse_dynamics(const pugi::xml_node& dynamics) {
    // Map common dynamics markings to MIDI velocity
    for (pugi::xml_node child : dynamics.children()) {
        std::string name = child.name();

        if (name == "ppp") return 16;
        else if (name == "pp") return 32;
        else if (name == "p") return 48;
        else if (name == "mp") return 56;
        else if (name == "mf") return 72;
        else if (name == "f") return 88;
        else if (name == "ff") return 104;
        else if (name == "fff") return 120;
    }

    return 64; // Default mezzo-forte
}

music_time_t musicxml_parser::calculate_measure_duration(const pugi::xml_node& measure, int divisions) {
    // This is a simplified calculation - real implementation would need time signatures
    return 480 * 4; // Assume 4/4 time for now
}

void musicxml_parser::parse_measure_content(const pugi::xml_node& part, uint8_t channel,
                                           music_time_t start_time, int divisions, music_data& data) {
    // Similar to parse_measure but for timewise format
    music_time_t current_time = start_time;

    for (pugi::xml_node child : part.children()) {
        std::string name = child.name();

        if (name == "note") {
            current_time += parse_note(child, channel, current_time, divisions, data);
        }
    }
}

#endif // HAVE_PUGIXML

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