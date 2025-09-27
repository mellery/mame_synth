#include "src/nes_playback_engine.h"
#include "src/music_parser.h"
#include <iostream>

int main() {
    try {
        std::cout << "Testing WAV export functionality..." << std::endl;

        // Create a simple test music data
        music_data test_music;
        test_music.metadata().ticks_per_quarter = 480;

        // Add a simple melody (C major scale)
        music_note notes[] = {
            {0, 60, 100, 0, 480},     // C4 for quarter note
            {0, 62, 100, 480, 480},   // D4 for quarter note
            {0, 64, 100, 960, 480},   // E4 for quarter note
            {0, 65, 100, 1440, 480},  // F4 for quarter note
            {0, 67, 100, 1920, 480},  // G4 for quarter note
        };

        for (auto& note : notes) {
            test_music.add_note(note);
        }

        std::cout << "Created test music with " << test_music.notes().size() << " notes" << std::endl;

        // Create playback engine
        nes_playback_engine::engine_config config;
        config.sample_rate = 44100;
        config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;

        nes_playback_engine engine(config);

        std::cout << "Created playback engine" << std::endl;

        // Load the test music
        if (!engine.load_music_data(test_music)) {
            std::cerr << "Failed to load music data" << std::endl;
            return 1;
        }

        std::cout << "Loaded music data successfully" << std::endl;

        // Export to WAV
        if (engine.export_to_wav("test_melody.wav", 44100)) {
            std::cout << "✓ WAV export successful! File: test_melody.wav" << std::endl;
        } else {
            std::cout << "✗ WAV export failed!" << std::endl;
            return 1;
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}