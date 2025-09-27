#include "test_framework.h"
#include "music_parser.h"
#include <vector>
#include <fstream>

REGISTER_TEST(music_parser, music_data_construction) {
    music_data data;

    ASSERT_TRUE(data.empty());
    ASSERT_EQ(0, data.note_count());
    ASSERT_EQ(0, data.event_count());
    ASSERT_EQ(480, data.metadata().ticks_per_quarter);
}

REGISTER_TEST(music_parser, music_data_add_note) {
    music_data data;

    music_note note1(0, 60, 100, 0, 480);
    music_note note2(1, 64, 80, 480, 240);

    data.add_note(note1);
    data.add_note(note2);

    ASSERT_FALSE(data.empty());
    ASSERT_EQ(2, data.note_count());
    ASSERT_EQ(2, data.event_count());
}

REGISTER_TEST(music_parser, music_data_add_events) {
    music_data data;

    // Add various event types
    data.add_note(music_note(0, 60, 100, 0, 480));
    data.add_tempo(music_tempo(500000, 0)); // 120 BPM
    data.add_program(music_program(0, 1, 0));
    data.add_control(music_control(0, 7, 100));

    ASSERT_EQ(1, data.note_count());
    ASSERT_EQ(4, data.event_count());
}

REGISTER_TEST(music_parser, music_note_construction) {
    music_note note(2, 67, 100, 960, 480);

    ASSERT_EQ(2, note.channel);
    ASSERT_EQ(67, note.note);
    ASSERT_EQ(100, note.velocity);
    ASSERT_EQ(960, note.start);
    ASSERT_EQ(480, note.duration);
}

REGISTER_TEST(music_parser, music_tempo_construction) {
    music_tempo tempo(500000, 0); // 120 BPM

    ASSERT_EQ(500000, tempo.microseconds_per_quarter);
    ASSERT_EQ(0, tempo.time);

    // Test BPM calculation
    double bpm = 60000000.0 / tempo.microseconds_per_quarter;
    ASSERT_NEAR(120.0, bpm, 0.01);
}

REGISTER_TEST(music_parser, music_program_construction) {
    music_program program(5, 42, 1920);

    ASSERT_EQ(5, program.channel);
    ASSERT_EQ(42, program.program);
    ASSERT_EQ(1920, program.time);
}

REGISTER_TEST(music_parser, music_control_construction) {
    music_control control(3, 7, 64, 2400);

    ASSERT_EQ(3, control.channel);
    ASSERT_EQ(7, control.controller);
    ASSERT_EQ(64, control.value);
    ASSERT_EQ(2400, control.time);
}

REGISTER_TEST(music_parser, midi_parser_creation) {
    midi_parser parser;

    ASSERT_TRUE(parser.supports_extension(".mid"));
    ASSERT_TRUE(parser.supports_extension(".midi"));
    ASSERT_TRUE(parser.supports_extension(".MID"));
    ASSERT_FALSE(parser.supports_extension(".xml"));
}

REGISTER_TEST(music_parser, musicxml_parser_creation) {
    musicxml_parser parser;

    ASSERT_TRUE(parser.supports_extension(".xml"));
    ASSERT_TRUE(parser.supports_extension(".musicxml"));
    ASSERT_TRUE(parser.supports_extension(".XML"));
    ASSERT_FALSE(parser.supports_extension(".mid"));
}

REGISTER_TEST(music_parser, parser_factory) {
    // Test MIDI parser creation
    auto midi_parser = music_parser_factory::create_parser("test.mid");
    ASSERT_NE(nullptr, midi_parser.get());

    auto midi_parser2 = music_parser_factory::create_parser("test.MIDI");
    ASSERT_NE(nullptr, midi_parser2.get());

    // Test MusicXML parser creation
    auto xml_parser = music_parser_factory::create_parser("test.xml");
    ASSERT_NE(nullptr, xml_parser.get());

    auto xml_parser2 = music_parser_factory::create_parser("test.musicxml");
    ASSERT_NE(nullptr, xml_parser2.get());

    // Test unsupported format
    auto null_parser = music_parser_factory::create_parser("test.wav");
    ASSERT_EQ(nullptr, null_parser.get());
}

REGISTER_TEST(music_parser, factory_supported_extensions) {
    auto extensions = music_parser_factory::supported_extensions();

    ASSERT_EQ(4, extensions.size());

    // Check that expected extensions are present
    bool found_mid = false, found_midi = false, found_xml = false, found_musicxml = false;

    for (const auto& ext : extensions) {
        if (ext == ".mid") found_mid = true;
        if (ext == ".midi") found_midi = true;
        if (ext == ".xml") found_xml = true;
        if (ext == ".musicxml") found_musicxml = true;
    }

    ASSERT_TRUE(found_mid);
    ASSERT_TRUE(found_midi);
    ASSERT_TRUE(found_xml);
    ASSERT_TRUE(found_musicxml);
}

REGISTER_TEST(music_parser, factory_is_supported) {
    ASSERT_TRUE(music_parser_factory::is_supported("song.mid"));
    ASSERT_TRUE(music_parser_factory::is_supported("song.MIDI"));
    ASSERT_TRUE(music_parser_factory::is_supported("song.xml"));
    ASSERT_TRUE(music_parser_factory::is_supported("song.musicxml"));
    ASSERT_FALSE(music_parser_factory::is_supported("song.wav"));
    ASSERT_FALSE(music_parser_factory::is_supported("song.txt"));
}

REGISTER_TEST(music_parser, midi_buffer_parsing_empty) {
    midi_parser parser;
    music_data data;

    std::vector<uint8_t> empty_buffer;
    ASSERT_FALSE(parser.parse_buffer(empty_buffer, data));
    ASSERT_FALSE(parser.get_last_error().empty());
}

REGISTER_TEST(music_parser, midi_buffer_parsing_invalid_header) {
    midi_parser parser;
    music_data data;

    // Invalid header
    std::vector<uint8_t> invalid_buffer = {'X', 'Y', 'Z', 'W', 0, 0, 0, 6, 0, 0, 0, 1, 1, 0xE0};
    ASSERT_FALSE(parser.parse_buffer(invalid_buffer, data));
    ASSERT_NE(std::string::npos, parser.get_last_error().find("Invalid MIDI file header"));
}

REGISTER_TEST(music_parser, midi_buffer_parsing_too_small) {
    midi_parser parser;
    music_data data;

    // Buffer too small (< 14 bytes)
    std::vector<uint8_t> small_buffer = {'M', 'T', 'h', 'd', 0, 0};
    ASSERT_FALSE(parser.parse_buffer(small_buffer, data));
    ASSERT_NE(std::string::npos, parser.get_last_error().find("too small"));
}

REGISTER_TEST(music_parser, create_test_midi_file) {
    // Create a simple test MIDI file for parsing
    std::vector<uint8_t> midi_data;

    // MIDI header "MThd"
    midi_data.insert(midi_data.end(), {'M', 'T', 'h', 'd'});

    // Header length (6 bytes)
    midi_data.insert(midi_data.end(), {0x00, 0x00, 0x00, 0x06});

    // Format type 0, 1 track, 480 ticks per quarter
    midi_data.insert(midi_data.end(), {0x00, 0x00, 0x00, 0x01, 0x01, 0xE0});

    // Track header "MTrk"
    midi_data.insert(midi_data.end(), {'M', 'T', 'r', 'k'});

    // Track data
    std::vector<uint8_t> track_data = {
        0x00,             // Delta time 0
        0x90, 0x3C, 0x64, // Note on C4, velocity 100
        0x83, 0x60,       // Delta time 480 (quarter note)
        0x80, 0x3C, 0x40, // Note off C4, velocity 64
        0x00,             // Delta time 0
        0xFF, 0x2F, 0x00  // End of track
    };

    // Track length
    uint32_t track_length = track_data.size();
    midi_data.push_back((track_length >> 24) & 0xFF);
    midi_data.push_back((track_length >> 16) & 0xFF);
    midi_data.push_back((track_length >> 8) & 0xFF);
    midi_data.push_back(track_length & 0xFF);

    // Track data
    midi_data.insert(midi_data.end(), track_data.begin(), track_data.end());

    // Write to file for parsing test
    std::ofstream file("test_simple.mid", std::ios::binary);
    file.write(reinterpret_cast<const char*>(midi_data.data()), midi_data.size());
    file.close();

    // Test parsing
    midi_parser parser;
    music_data data;

    ASSERT_TRUE(parser.parse_file("test_simple.mid", data));
    ASSERT_EQ(1, data.note_count());
    ASSERT_EQ(480, data.metadata().ticks_per_quarter);
}