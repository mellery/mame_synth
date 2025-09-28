#pragma once

// MIDI/MusicXML parser interface for MAME Synthesizer
// Provides unified abstraction for reading different music file formats

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#ifdef HAVE_PUGIXML
#include <pugixml.hpp>
#endif

// Forward declarations
class music_event;
class music_track;
class music_file;

// Time representation in ticks (compatible with MIDI timing)
using music_time_t = uint32_t;

// Note representation
struct music_note {
    uint8_t channel;      // MIDI channel (0-15)
    uint8_t note;         // MIDI note number (0-127)
    uint8_t velocity;     // Note velocity (0-127)
    music_time_t start;   // Start time in ticks
    music_time_t duration; // Duration in ticks

    music_note(uint8_t ch = 0, uint8_t n = 60, uint8_t vel = 64,
               music_time_t st = 0, music_time_t dur = 480)
        : channel(ch), note(n), velocity(vel), start(st), duration(dur) {}
};

// Control change event
struct music_control {
    uint8_t channel;      // MIDI channel (0-15)
    uint8_t controller;   // Controller number (0-127)
    uint8_t value;        // Controller value (0-127)
    music_time_t time;    // Event time in ticks

    music_control(uint8_t ch = 0, uint8_t ctrl = 0, uint8_t val = 0, music_time_t t = 0)
        : channel(ch), controller(ctrl), value(val), time(t) {}
};

// Program change event
struct music_program {
    uint8_t channel;      // MIDI channel (0-15)
    uint8_t program;      // Program number (0-127)
    music_time_t time;    // Event time in ticks

    music_program(uint8_t ch = 0, uint8_t prog = 0, music_time_t t = 0)
        : channel(ch), program(prog), time(t) {}
};

// Tempo change event
struct music_tempo {
    uint32_t microseconds_per_quarter; // Microseconds per quarter note
    music_time_t time;                  // Event time in ticks

    music_tempo(uint32_t tempo = 500000, music_time_t t = 0)
        : microseconds_per_quarter(tempo), time(t) {}

    // Helper to get BPM
    double get_bpm() const {
        return 60000000.0 / microseconds_per_quarter;
    }
};

// Music file metadata
struct music_metadata {
    std::string title;
    std::string composer;
    std::string copyright;
    uint16_t ticks_per_quarter = 480; // Default MIDI resolution
    uint16_t track_count = 0;
    music_time_t total_ticks = 0;

    // Helper to convert ticks to milliseconds
    double ticks_to_ms(music_time_t ticks, uint32_t tempo_uspq = 500000) const {
        return (double(ticks) * tempo_uspq) / (ticks_per_quarter * 1000.0);
    }
};

// Parsed music data container
class music_data {
public:
    music_data() = default;
    ~music_data() = default;

    // Add events
    void add_note(const music_note& note) { m_notes.push_back(note); }
    void add_control(const music_control& ctrl) { m_controls.push_back(ctrl); }
    void add_program(const music_program& prog) { m_programs.push_back(prog); }
    void add_tempo(const music_tempo& tempo) { m_tempos.push_back(tempo); }

    // Getters
    const std::vector<music_note>& notes() const { return m_notes; }
    const std::vector<music_control>& controls() const { return m_controls; }
    const std::vector<music_program>& programs() const { return m_programs; }
    const std::vector<music_tempo>& tempos() const { return m_tempos; }
    const music_metadata& metadata() const { return m_metadata; }
    music_metadata& metadata() { return m_metadata; }

    // Statistics
    size_t note_count() const { return m_notes.size(); }
    size_t event_count() const { return m_notes.size() + m_controls.size() + m_programs.size() + m_tempos.size(); }
    bool empty() const { return event_count() == 0; }

    // Clear all data
    void clear() {
        m_notes.clear();
        m_controls.clear();
        m_programs.clear();
        m_tempos.clear();
        m_metadata = {};
    }

private:
    std::vector<music_note> m_notes;
    std::vector<music_control> m_controls;
    std::vector<music_program> m_programs;
    std::vector<music_tempo> m_tempos;
    music_metadata m_metadata;
};

// Abstract base parser interface
class music_parser {
public:
    virtual ~music_parser() = default;

    // Parse music file and return structured data
    virtual bool parse_file(const std::string& filename, music_data& data) = 0;

    // Parse from memory buffer
    virtual bool parse_buffer(const std::vector<uint8_t>& buffer, music_data& data) = 0;

    // Get last error message
    virtual std::string get_last_error() const = 0;

    // Check if parser supports file extension
    virtual bool supports_extension(const std::string& ext) const = 0;

protected:
    std::string m_last_error;

    void set_error(const std::string& error) { m_last_error = error; }
};

// MIDI file parser implementation
class midi_parser : public music_parser {
public:
    midi_parser();
    virtual ~midi_parser() = default;

    bool parse_file(const std::string& filename, music_data& data) override;
    bool parse_buffer(const std::vector<uint8_t>& buffer, music_data& data) override;
    std::string get_last_error() const override { return m_last_error; }
    bool supports_extension(const std::string& ext) const override;

private:
    bool parse_midi_buffer(const uint8_t* data, size_t size, music_data& output);
    bool parse_header_chunk(const uint8_t* data, size_t size, music_metadata& meta);
    bool parse_track_chunk(const uint8_t* data, size_t size, music_data& output);
    uint32_t read_variable_length(const uint8_t*& data, size_t& remaining);
    uint32_t read_uint32_be(const uint8_t* data);
    uint16_t read_uint16_be(const uint8_t* data);
};

// MusicXML parser implementation
class musicxml_parser : public music_parser {
public:
    musicxml_parser();
    virtual ~musicxml_parser() = default;

    bool parse_file(const std::string& filename, music_data& data) override;
    bool parse_buffer(const std::vector<uint8_t>& buffer, music_data& data) override;
    std::string get_last_error() const override { return m_last_error; }
    bool supports_extension(const std::string& ext) const override;

private:
#ifdef HAVE_PUGIXML

    // Internal structure for part information
    struct part_info {
        std::string id;
        std::string name;
        int midi_channel = 0;
        int midi_program = 0;
    };

    // Main parsing methods
    bool parse_musicxml_document(const pugi::xml_document& doc, music_data& data);
    void parse_work_info(const pugi::xml_node& root, music_metadata& metadata);
    bool parse_part_list(const pugi::xml_node& part_list, std::vector<part_info>& parts);
    bool parse_partwise_score(const pugi::xml_node& root, const std::vector<part_info>& parts, music_data& data);
    bool parse_timewise_score(const pugi::xml_node& root, const std::vector<part_info>& parts, music_data& data);

    // Measure and note parsing
    music_time_t parse_measure(const pugi::xml_node& measure, uint8_t channel,
                               music_time_t start_time, int& divisions, music_data& data);
    music_time_t parse_note(const pugi::xml_node& note, uint8_t channel,
                            music_time_t start_time, int divisions, music_data& data);
    void parse_measure_content(const pugi::xml_node& part, uint8_t channel,
                               music_time_t start_time, int divisions, music_data& data);

    // Utility methods
    uint8_t convert_pitch_to_midi(const std::string& step, int octave, int alter);
    uint8_t parse_dynamics(const pugi::xml_node& dynamics);
    music_time_t calculate_measure_duration(const pugi::xml_node& measure, int divisions);
#endif
};

// Parser factory for automatic format detection
class music_parser_factory {
public:
    // Create appropriate parser based on file extension
    static std::unique_ptr<music_parser> create_parser(const std::string& filename);

    // Get list of supported extensions
    static std::vector<std::string> supported_extensions();

    // Check if file format is supported
    static bool is_supported(const std::string& filename);
};