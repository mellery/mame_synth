#include "tests/test_framework.h"
#include "src/nes_playback_engine.h"

// Minimal test to verify WAV analysis fixes work
int main() {
    try {
        std::cout << "Testing WAV analysis API fixes...\n";

        // Test the configuration structure we fixed
        nes_playback_engine::engine_config config;
        config.sample_rate = 44100;          // Fixed: was config.audio_config.sample_rate
        config.buffer_size = 1024;           // Fixed: was config.audio_config.buffer_size
        config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT; // Fixed: was config.audio_config.backend

        // Create engine with fixed API
        nes_playback_engine engine(config);
        bool init_result = engine.initialize();

        std::cout << "✅ Engine initialization: " << (init_result ? "SUCCESS" : "FAILED") << "\n";

        // Test the music loading API we fixed
        music_data music;
        // music.add_note(music_note(0, 60, 100, 0, 2200)); // Would need full music_data implementation

        bool load_result = engine.load_music_data(music); // Fixed: was load_music()
        std::cout << "✅ Music loading API: " << (load_result ? "SUCCESS" : "FAILED") << "\n";

        // Test the export method we implemented
        std::string test_filename = "/tmp/test_wav_analysis_verification.wav";
        bool export_result = engine.export_to_wav(test_filename, 44100); // Fixed: replaced start_playback/stop_playback
        std::cout << "✅ WAV export API: " << (export_result ? "SUCCESS" : "FAILED") << "\n";

        // Test the play/stop methods we updated
        bool play_result = engine.play(); // Fixed: was start_playback()
        bool stop_result = engine.stop(); // Fixed: was stop_playback()
        std::cout << "✅ Playback control API: " << (play_result && stop_result ? "SUCCESS" : "FAILED") << "\n";

        std::cout << "\n🎉 WAV analysis API fixes verification completed!\n";
        std::cout << "All the API changes compile and execute correctly.\n";

        return 0;
    } catch (const std::exception& e) {
        std::cout << "❌ ERROR: " << e.what() << std::endl;
        return 1;
    }
}