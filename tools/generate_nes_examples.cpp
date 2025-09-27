#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <map>

/**
 * NES MIDI Example Generator
 *
 * Generates comprehensive MIDI file examples showcasing NES APU capabilities.
 * Creates files that demonstrate each channel type, NES-specific techniques,
 * and musical compositions optimized for NES hardware.
 */

class MIDIGenerator {
public:
    struct MIDIEvent {
        uint32_t delta_time;
        std::vector<uint8_t> data;
    };

    struct Track {
        std::vector<MIDIEvent> events;
        std::string name;
    };

private:
    std::vector<Track> tracks;
    uint16_t ticks_per_quarter = 480;

public:
    void set_ticks_per_quarter(uint16_t ticks) {
        ticks_per_quarter = ticks;
    }

    void add_track(const Track& track) {
        tracks.push_back(track);
    }

    void write_file(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot create file: " + filename);
        }

        // Write MIDI header
        write_header(file);

        // Write tracks
        for (const auto& track : tracks) {
            write_track(file, track);
        }

        file.close();
        std::cout << "Generated: " << filename << std::endl;
    }

private:
    void write_header(std::ofstream& file) {
        // Header chunk
        file.write("MThd", 4);
        write_uint32(file, 6); // Header length
        write_uint16(file, tracks.size() > 1 ? 1 : 0); // Format (0 or 1)
        write_uint16(file, tracks.size()); // Number of tracks
        write_uint16(file, ticks_per_quarter); // Ticks per quarter note
    }

    void write_track(std::ofstream& file, const Track& track) {
        // Calculate track data
        std::vector<uint8_t> track_data;
        for (const auto& event : track.events) {
            write_variable_length(track_data, event.delta_time);
            track_data.insert(track_data.end(), event.data.begin(), event.data.end());
        }

        // Track header
        file.write("MTrk", 4);
        write_uint32(file, track_data.size());

        // Track data
        file.write(reinterpret_cast<const char*>(track_data.data()), track_data.size());
    }

    void write_uint32(std::ofstream& file, uint32_t value) {
        file.put((value >> 24) & 0xFF);
        file.put((value >> 16) & 0xFF);
        file.put((value >> 8) & 0xFF);
        file.put(value & 0xFF);
    }

    void write_uint16(std::ofstream& file, uint16_t value) {
        file.put((value >> 8) & 0xFF);
        file.put(value & 0xFF);
    }

    void write_variable_length(std::vector<uint8_t>& data, uint32_t value) {
        if (value >= 0x200000) {
            data.push_back(((value >> 21) & 0x7F) | 0x80);
        }
        if (value >= 0x4000) {
            data.push_back(((value >> 14) & 0x7F) | 0x80);
        }
        if (value >= 0x80) {
            data.push_back(((value >> 7) & 0x7F) | 0x80);
        }
        data.push_back(value & 0x7F);
    }
};

// Helper functions for creating MIDI events
std::vector<uint8_t> note_on(uint8_t channel, uint8_t note, uint8_t velocity) {
    return {static_cast<uint8_t>(0x90 | channel), note, velocity};
}

std::vector<uint8_t> note_off(uint8_t channel, uint8_t note, uint8_t velocity = 64) {
    return {static_cast<uint8_t>(0x80 | channel), note, velocity};
}

std::vector<uint8_t> control_change(uint8_t channel, uint8_t controller, uint8_t value) {
    return {static_cast<uint8_t>(0xB0 | channel), controller, value};
}

std::vector<uint8_t> program_change(uint8_t channel, uint8_t program) {
    return {static_cast<uint8_t>(0xC0 | channel), program};
}

std::vector<uint8_t> pitch_bend(uint8_t channel, uint16_t bend) {
    return {static_cast<uint8_t>(0xE0 | channel),
            static_cast<uint8_t>(bend & 0x7F),
            static_cast<uint8_t>((bend >> 7) & 0x7F)};
}

std::vector<uint8_t> track_name(const std::string& name) {
    std::vector<uint8_t> event = {0xFF, 0x03, static_cast<uint8_t>(name.length())};
    event.insert(event.end(), name.begin(), name.end());
    return event;
}

std::vector<uint8_t> end_of_track() {
    return {0xFF, 0x2F, 0x00};
}

// NES note mappings (NTSC frequencies)
std::map<std::string, uint8_t> nes_notes = {
    // Standard chromatic notes optimized for NES
    {"C3", 48}, {"C#3", 49}, {"D3", 50}, {"D#3", 51}, {"E3", 52}, {"F3", 53},
    {"F#3", 54}, {"G3", 55}, {"G#3", 56}, {"A3", 57}, {"A#3", 58}, {"B3", 59},
    {"C4", 60}, {"C#4", 61}, {"D4", 62}, {"D#4", 63}, {"E4", 64}, {"F4", 65},
    {"F#4", 66}, {"G4", 67}, {"G#4", 68}, {"A4", 69}, {"A#4", 70}, {"B4", 71},
    {"C5", 72}, {"C#5", 73}, {"D5", 74}, {"D#5", 75}, {"E5", 76}, {"F5", 77},
    {"F#5", 78}, {"G5", 79}, {"G#5", 80}, {"A5", 81}, {"A#5", 82}, {"B5", 83},
    {"C6", 84}, {"C#6", 85}, {"D6", 86}, {"D#6", 87}, {"E6", 88}, {"F6", 89}
};

void generate_basic_channel_demos() {
    std::cout << "\nGenerating basic channel demonstration files...\n";

    // Pulse Channel 1 Demo
    {
        MIDIGenerator midi;
        MIDIGenerator::Track track;
        track.name = "Pulse 1 Demo";

        track.events.push_back({0, track_name("NES Pulse Channel 1 - Duty Cycles")});

        // Demonstrate different duty cycles via different notes/velocities
        std::vector<std::pair<std::string, uint8_t>> notes = {
            {"C4", 100}, {"E4", 80}, {"G4", 60}, {"C5", 40}
        };

        uint32_t time = 0;
        for (const auto& [note_name, velocity] : notes) {
            uint8_t note = nes_notes[note_name];
            track.events.push_back({time, note_on(0, note, velocity)});
            track.events.push_back({480, note_off(0, note)});
            time = 120; // Gap between notes
        }

        track.events.push_back({240, end_of_track()});
        midi.add_track(track);
        midi.write_file("examples/midi/basic/pulse1_demo.mid");
    }

    // Pulse Channel 2 Demo
    {
        MIDIGenerator midi;
        MIDIGenerator::Track track;
        track.name = "Pulse 2 Demo";

        track.events.push_back({0, track_name("NES Pulse Channel 2 - Harmony")});

        // Simple harmony with pulse 1
        std::vector<std::string> notes = {"E4", "G4", "B4", "E5"};

        uint32_t time = 0;
        for (const auto& note_name : notes) {
            uint8_t note = nes_notes[note_name];
            track.events.push_back({time, note_on(1, note, 80)});
            track.events.push_back({480, note_off(1, note)});
            time = 120;
        }

        track.events.push_back({240, end_of_track()});
        midi.add_track(track);
        midi.write_file("examples/midi/basic/pulse2_demo.mid");
    }

    // Triangle Channel Demo
    {
        MIDIGenerator midi;
        MIDIGenerator::Track track;
        track.name = "Triangle Demo";

        track.events.push_back({0, track_name("NES Triangle Channel - Bass Line")});

        // Bass line pattern
        std::vector<std::string> bass_notes = {"C3", "G3", "A3", "F3", "C3", "E3", "F3", "G3"};

        uint32_t time = 0;
        for (const auto& note_name : bass_notes) {
            uint8_t note = nes_notes[note_name];
            track.events.push_back({time, note_on(2, note, 100)});
            track.events.push_back({240, note_off(2, note)});
            time = 240;
        }

        track.events.push_back({240, end_of_track()});
        midi.add_track(track);
        midi.write_file("examples/midi/basic/triangle_demo.mid");
    }

    // Noise Channel Demo
    {
        MIDIGenerator midi;
        MIDIGenerator::Track track;
        track.name = "Noise Demo";

        track.events.push_back({0, track_name("NES Noise Channel - Percussion")});

        // Drum pattern using different pitches for different noise types
        std::vector<std::pair<uint8_t, uint8_t>> drums = {
            {36, 100}, // Kick drum (low noise)
            {42, 80},  // Hi-hat (high noise)
            {38, 90},  // Snare (mid noise)
            {42, 60}   // Hi-hat (softer)
        };

        uint32_t time = 0;
        for (int bar = 0; bar < 4; ++bar) {
            for (const auto& [drum_note, velocity] : drums) {
                track.events.push_back({time, note_on(9, drum_note, velocity)});
                track.events.push_back({60, note_off(9, drum_note)});
                time = 180;
            }
        }

        track.events.push_back({240, end_of_track()});
        midi.add_track(track);
        midi.write_file("examples/midi/basic/noise_demo.mid");
    }

    // DMC Channel Demo
    {
        MIDIGenerator midi;
        MIDIGenerator::Track track;
        track.name = "DMC Demo";

        track.events.push_back({0, track_name("NES DMC Channel - Samples")});

        // Low frequency notes for DMC channel
        std::vector<std::string> dmc_notes = {"C2", "D2", "E2", "F2", "G2", "A2", "B2", "C3"};

        uint32_t time = 0;
        for (const auto& note_name : dmc_notes) {
            uint8_t note = nes_notes.count(note_name) ? nes_notes[note_name] - 24 : 24; // Lower octave
            track.events.push_back({time, note_on(3, note, 90)});
            track.events.push_back({360, note_off(3, note)});
            time = 120;
        }

        track.events.push_back({240, end_of_track()});
        midi.add_track(track);
        midi.write_file("examples/midi/basic/dmc_demo.mid");
    }
}

void generate_technique_examples() {
    std::cout << "\nGenerating NES technique demonstration files...\n";

    // Arpeggios Demo
    {
        MIDIGenerator midi;
        MIDIGenerator::Track track;
        track.name = "Arpeggio Demo";

        track.events.push_back({0, track_name("NES Arpeggio Techniques")});

        // Fast arpeggios using pulse channels
        std::vector<std::vector<std::string>> arpeggios = {
            {"C4", "E4", "G4"}, // C major
            {"D4", "F#4", "A4"}, // D major
            {"E4", "G#4", "B4"}, // E major
            {"F4", "A4", "C5"}   // F major
        };

        uint32_t time = 0;
        for (const auto& arp : arpeggios) {
            for (const auto& note_name : arp) {
                uint8_t note = nes_notes[note_name];
                track.events.push_back({time, note_on(0, note, 90)});
                track.events.push_back({80, note_off(0, note)});
                time = 80;
            }
            time = 160; // Gap between arpeggios
        }

        track.events.push_back({240, end_of_track()});
        midi.add_track(track);
        midi.write_file("examples/midi/techniques/arpeggios.mid");
    }

    // Pitch Slides Demo
    {
        MIDIGenerator midi;
        MIDIGenerator::Track track;
        track.name = "Pitch Slides";

        track.events.push_back({0, track_name("NES Pitch Slide Effects")});

        // Pitch bend effects
        track.events.push_back({0, note_on(0, nes_notes["C4"], 100)});

        // Slide up
        for (int i = 0; i <= 16; ++i) {
            uint16_t bend = 8192 + (i * 256); // Bend up
            track.events.push_back({30, pitch_bend(0, bend)});
        }

        track.events.push_back({480, note_off(0, nes_notes["C4"])});

        // Reset pitch bend
        track.events.push_back({120, pitch_bend(0, 8192)});

        track.events.push_back({240, end_of_track()});
        midi.add_track(track);
        midi.write_file("examples/midi/techniques/pitch_slides.mid");
    }

    // Echo Effects Demo
    {
        MIDIGenerator midi;
        MIDIGenerator::Track track;
        track.name = "Echo Effects";

        track.events.push_back({0, track_name("NES Echo and Delay Effects")});

        // Echo effect using multiple channels
        std::string melody_note = "C5";
        uint8_t note = nes_notes[melody_note];

        // Original note on pulse 1
        track.events.push_back({0, note_on(0, note, 100)});
        track.events.push_back({480, note_off(0, note)});

        // Echo on pulse 2 (delayed and quieter)
        track.events.push_back({240, note_on(1, note, 60)});
        track.events.push_back({480, note_off(1, note)});

        // Second echo (even quieter)
        track.events.push_back({240, note_on(0, note, 30)});
        track.events.push_back({480, note_off(0, note)});

        track.events.push_back({240, end_of_track()});
        midi.add_track(track);
        midi.write_file("examples/midi/techniques/echo_effects.mid");
    }
}

void generate_musical_compositions() {
    std::cout << "\nGenerating musical composition examples...\n";

    // Simple Melody
    {
        MIDIGenerator midi;
        MIDIGenerator::Track melody_track;
        melody_track.name = "Melody";

        melody_track.events.push_back({0, track_name("Simple NES Melody")});

        // "Twinkle Twinkle Little Star" style melody
        std::vector<std::pair<std::string, uint32_t>> melody = {
            {"C4", 480}, {"C4", 480}, {"G4", 480}, {"G4", 480},
            {"A4", 480}, {"A4", 480}, {"G4", 960},
            {"F4", 480}, {"F4", 480}, {"E4", 480}, {"E4", 480},
            {"D4", 480}, {"D4", 480}, {"C4", 960}
        };

        uint32_t time = 0;
        for (const auto& [note_name, duration] : melody) {
            uint8_t note = nes_notes[note_name];
            melody_track.events.push_back({time, note_on(0, note, 90)});
            melody_track.events.push_back({duration - 60, note_off(0, note)});
            time = 60;
        }

        melody_track.events.push_back({240, end_of_track()});
        midi.add_track(melody_track);

        // Bass line
        MIDIGenerator::Track bass_track;
        bass_track.name = "Bass";

        bass_track.events.push_back({0, track_name("Bass Line")});

        std::vector<std::pair<std::string, uint32_t>> bass = {
            {"C3", 1920}, {"F3", 1920}, {"G3", 1920}, {"C3", 1920}
        };

        time = 0;
        for (const auto& [note_name, duration] : bass) {
            uint8_t note = nes_notes[note_name];
            bass_track.events.push_back({time, note_on(2, note, 100)});
            bass_track.events.push_back({duration - 120, note_off(2, note)});
            time = 120;
        }

        bass_track.events.push_back({240, end_of_track()});
        midi.add_track(bass_track);

        midi.write_file("examples/midi/compositions/simple_melody.mid");
    }

    // Chiptune Style
    {
        MIDIGenerator midi;

        // Lead melody
        MIDIGenerator::Track lead_track;
        lead_track.name = "Lead";
        lead_track.events.push_back({0, track_name("Chiptune Lead")});

        // Fast 16th note melody
        std::vector<std::string> lead_notes = {
            "C5", "D5", "E5", "F5", "G5", "F5", "E5", "D5",
            "C5", "E5", "G5", "E5", "C5", "G4", "C5", "G4"
        };

        uint32_t time = 0;
        for (const auto& note_name : lead_notes) {
            uint8_t note = nes_notes[note_name];
            lead_track.events.push_back({time, note_on(0, note, 95)});
            lead_track.events.push_back({110, note_off(0, note)});
            time = 120;
        }
        lead_track.events.push_back({240, end_of_track()});
        midi.add_track(lead_track);

        // Harmony
        MIDIGenerator::Track harmony_track;
        harmony_track.name = "Harmony";
        harmony_track.events.push_back({0, track_name("Harmony")});

        std::vector<std::string> harmony_notes = {
            "E4", "F4", "G4", "A4", "B4", "A4", "G4", "F4"
        };

        time = 0;
        for (const auto& note_name : harmony_notes) {
            uint8_t note = nes_notes[note_name];
            harmony_track.events.push_back({time, note_on(1, note, 70)});
            harmony_track.events.push_back({230, note_off(1, note)});
            time = 240;
        }
        harmony_track.events.push_back({240, end_of_track()});
        midi.add_track(harmony_track);

        midi.write_file("examples/midi/compositions/chiptune_style.mid");
    }
}

void generate_educational_examples() {
    std::cout << "\nGenerating educational and reference files...\n";

    // All Channels Demo
    {
        MIDIGenerator midi;

        // Create tracks for each NES channel
        std::vector<std::pair<int, std::string>> channels = {
            {0, "Pulse 1"}, {1, "Pulse 2"}, {2, "Triangle"}, {9, "Noise"}, {3, "DMC"}
        };

        for (const auto& [channel, name] : channels) {
            MIDIGenerator::Track track;
            track.name = name;
            track.events.push_back({0, track_name(name + " Channel")});

            if (channel == 9) { // Noise channel
                // Drum pattern
                std::vector<uint8_t> drums = {36, 42, 38, 42};
                uint32_t time = 0;
                for (int i = 0; i < 8; ++i) {
                    uint8_t drum = drums[i % 4];
                    track.events.push_back({time, note_on(9, drum, 80)});
                    track.events.push_back({110, note_off(9, drum)});
                    time = 120;
                }
            } else {
                // Melodic pattern
                std::vector<std::string> pattern = {"C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"};
                uint32_t time = 0;
                for (const auto& note_name : pattern) {
                    uint8_t note = nes_notes[note_name];
                    if (channel == 2 || channel == 3) note -= 12; // Lower octave for triangle/DMC
                    track.events.push_back({time, note_on(channel, note, 85)});
                    track.events.push_back({110, note_off(channel, note)});
                    time = 120;
                }
            }

            track.events.push_back({240, end_of_track()});
            midi.add_track(track);
        }

        midi.write_file("examples/midi/educational/all_channels.mid");
    }

    // Note Range Demo
    {
        MIDIGenerator midi;
        MIDIGenerator::Track track;
        track.name = "Note Range";

        track.events.push_back({0, track_name("NES Note Range Demo")});

        // Chromatic scale across NES range
        uint32_t time = 0;
        for (uint8_t note = 36; note <= 96; note += 2) { // Every other note for speed
            track.events.push_back({time, note_on(0, note, 70)});
            track.events.push_back({80, note_off(0, note)});
            time = 80;
        }

        track.events.push_back({240, end_of_track()});
        midi.add_track(track);
        midi.write_file("examples/midi/educational/note_range.mid");
    }
}

int main() {
    std::cout << "NES MIDI Example Generator\n";
    std::cout << "==========================\n";

    try {
        generate_basic_channel_demos();
        generate_technique_examples();
        generate_musical_compositions();
        generate_educational_examples();

        std::cout << "\nAll NES MIDI examples generated successfully!\n";
        std::cout << "\nGenerated files:\n";
        std::cout << "  Basic Examples:\n";
        std::cout << "    - pulse1_demo.mid\n";
        std::cout << "    - pulse2_demo.mid\n";
        std::cout << "    - triangle_demo.mid\n";
        std::cout << "    - noise_demo.mid\n";
        std::cout << "    - dmc_demo.mid\n";
        std::cout << "  Technique Examples:\n";
        std::cout << "    - arpeggios.mid\n";
        std::cout << "    - pitch_slides.mid\n";
        std::cout << "    - echo_effects.mid\n";
        std::cout << "  Musical Compositions:\n";
        std::cout << "    - simple_melody.mid\n";
        std::cout << "    - chiptune_style.mid\n";
        std::cout << "  Educational Examples:\n";
        std::cout << "    - all_channels.mid\n";
        std::cout << "    - note_range.mid\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}