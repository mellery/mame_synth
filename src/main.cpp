#include <iostream>
#include "machine_stub.h"
#include "music_parser.h"

// Test MAME integration with machine context stub and music parser
int main() {
    std::cout << "MAME Synth - Music Parser Interface Test" << std::endl;

    try {
        // Create and initialize machine context stub
        machine_stub machine;

        if (!machine.initialize()) {
            std::cerr << "Failed to initialize machine context" << std::endl;
            return 1;
        }

        // Test device factory creation
        audio_device_factory factory(machine);

        // Test device creation (currently stubbed)
        auto nes_apu = factory.create_nes_apu("nes_apu", 1789773);
        auto snes_dsp = factory.create_snes_dsp("snes_dsp", 32000);

        // Show machine configuration
        std::cout << "\nMachine Configuration:\n" << machine.get_config_info() << std::endl;

        // Clean up device handles
        factory.destroy_device(nes_apu);
        factory.destroy_device(snes_dsp);

        // Test music parser interface
        std::cout << "\n=== Testing Music Parser Interface ===" << std::endl;

        // Test parser factory
        auto supported = music_parser_factory::supported_extensions();
        std::cout << "Supported file formats: ";
        for (const auto& ext : supported) {
            std::cout << ext << " ";
        }
        std::cout << std::endl;

        // Test MIDI parser creation
        auto midi_parser = music_parser_factory::create_parser("test.mid");
        if (midi_parser) {
            std::cout << "MIDI parser created successfully" << std::endl;
        }

        // Test MusicXML parser creation
        auto xml_parser = music_parser_factory::create_parser("test.xml");
        if (xml_parser) {
            std::cout << "MusicXML parser created successfully" << std::endl;
        }

        // Test parsing with sample data
        music_data test_data;

        // Add some sample notes
        test_data.add_note(music_note(0, 60, 100, 0, 480));    // C4
        test_data.add_note(music_note(0, 64, 100, 480, 480));  // E4
        test_data.add_note(music_note(0, 67, 100, 960, 480));  // G4

        // Add tempo and program events
        test_data.add_tempo(music_tempo(500000, 0)); // 120 BPM
        test_data.add_program(music_program(0, 1, 0)); // Acoustic Piano

        std::cout << "Created test music data:" << std::endl;
        std::cout << "  Notes: " << test_data.note_count() << std::endl;
        std::cout << "  Total events: " << test_data.event_count() << std::endl;
        std::cout << "  Empty: " << (test_data.empty() ? "yes" : "no") << std::endl;

        // Test actual MIDI parsing
        if (midi_parser) {
            std::cout << "\nTesting MIDI file parsing..." << std::endl;
            music_data midi_data;
            if (midi_parser->parse_file("../test_basic.mid", midi_data)) {
                std::cout << "Successfully parsed MIDI file!" << std::endl;
                std::cout << "  Notes: " << midi_data.note_count() << std::endl;
                std::cout << "  Total events: " << midi_data.event_count() << std::endl;
                std::cout << "  Ticks per quarter: " << midi_data.metadata().ticks_per_quarter << std::endl;
            } else {
                std::cout << "MIDI parsing failed: " << midi_parser->get_last_error() << std::endl;
            }
        }

        std::cout << "\nMusic parser interface test completed successfully!" << std::endl;

        // Machine will auto-shutdown via destructor
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}