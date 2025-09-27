#include "src/nes_channel_assignment.h"
#include <iostream>
#include <iomanip>

using namespace nes_channel_assignment;

// Helper function to print assignment results
void print_assignment_result(const assignment_result& result) {
    std::cout << "=== Channel Assignment Results ===\n";
    std::cout << "Overall Quality: " << std::fixed << std::setprecision(2)
              << result.overall_quality_score << "/1.00\n\n";

    std::cout << "Assignments:\n";
    std::cout << "MIDI Ch | NES Ch | Type      | Confidence | Reason\n";
    std::cout << "--------|--------|-----------|------------|------------------\n";

    for (const auto& assignment : result.assignments) {
        std::string channel_name;
        switch (assignment.nes_channel) {
            case nes_channel_type::PULSE_1: channel_name = "Pulse1"; break;
            case nes_channel_type::PULSE_2: channel_name = "Pulse2"; break;
            case nes_channel_type::TRIANGLE: channel_name = "Triangle"; break;
            case nes_channel_type::NOISE: channel_name = "Noise"; break;
            case nes_channel_type::DMC: channel_name = "DMC"; break;
        }

        std::cout << std::setw(7) << static_cast<int>(assignment.midi_channel) << " | "
                  << std::setw(6) << static_cast<int>(assignment.nes_channel) << " | "
                  << std::setw(9) << channel_name << " | "
                  << std::setw(10) << std::fixed << std::setprecision(2) << assignment.confidence_score << " | "
                  << assignment.reason.substr(0, 20) << "\n";
    }

    if (!result.warnings.empty()) {
        std::cout << "\nWarnings:\n";
        for (const auto& warning : result.warnings) {
            std::cout << "  - " << warning << "\n";
        }
    }

    if (!result.suggestions.empty()) {
        std::cout << "\nSuggestions:\n";
        for (const auto& suggestion : result.suggestions) {
            std::cout << "  - " << suggestion << "\n";
        }
    }
    std::cout << "\n";
}

// Helper function to print track analysis
void print_track_analysis(const track_analysis& analysis) {
    std::cout << "MIDI Channel " << static_cast<int>(analysis.midi_channel) << ":\n";
    std::cout << "  Frequency Range: " << std::fixed << std::setprecision(1)
              << analysis.min_frequency_hz << " - " << analysis.max_frequency_hz << " Hz\n";
    std::cout << "  Average: " << analysis.avg_frequency_hz << " Hz\n";
    std::cout << "  Note Count: " << analysis.note_count << "\n";
    std::cout << "  Notes/Second: " << std::fixed << std::setprecision(2) << analysis.notes_per_second << "\n";
    std::cout << "  Fast Passages: " << (analysis.has_fast_passages ? "Yes" : "No") << "\n";
    std::cout << "  Sustained Notes: " << (analysis.has_sustained_notes ? "Yes" : "No") << "\n";
    std::cout << "  Dynamic Range: " << (analysis.has_dynamic_variation ? "Yes" : "No") << "\n";

    std::cout << "  Channel Suitability:\n";
    std::cout << "    Pulse 1:  " << std::fixed << std::setprecision(2) << analysis.pulse1_suitability << "\n";
    std::cout << "    Pulse 2:  " << analysis.pulse2_suitability << "\n";
    std::cout << "    Triangle: " << analysis.triangle_suitability << "\n";
    std::cout << "    Noise:    " << analysis.noise_suitability << "\n";
    std::cout << "    DMC:      " << analysis.dmc_suitability << "\n\n";
}

// Create sample music data for testing
music_data create_sample_music() {
    music_data music;

    // Add some sample notes for different MIDI channels

    // Channel 0: High melody (should go to Pulse 1)
    music.add_note(music_note(0, 72, 100, 0, 480));     // C5
    music.add_note(music_note(0, 74, 90, 480, 480));    // D5
    music.add_note(music_note(0, 76, 80, 960, 480));    // E5
    music.add_note(music_note(0, 77, 85, 1440, 480));   // F5

    // Channel 1: Mid-range harmony (should go to Pulse 2)
    music.add_note(music_note(1, 64, 70, 0, 960));      // E4 (sustained)
    music.add_note(music_note(1, 67, 75, 960, 960));    // G4 (sustained)

    // Channel 2: Bass line (should go to Triangle)
    music.add_note(music_note(2, 36, 127, 0, 480));     // C2
    music.add_note(music_note(2, 43, 127, 480, 480));   // G2
    music.add_note(music_note(2, 41, 127, 960, 480));   // F2
    music.add_note(music_note(2, 38, 127, 1440, 480));  // D2

    // Channel 9: Drums (should go to Noise)
    music.add_note(music_note(9, 36, 127, 0, 120));     // Kick
    music.add_note(music_note(9, 42, 80, 240, 120));    // Hi-hat
    music.add_note(music_note(9, 38, 100, 480, 120));   // Snare
    music.add_note(music_note(9, 42, 60, 720, 120));    // Hi-hat

    // Channel 3: Low accent notes (should go to DMC)
    music.add_note(music_note(3, 24, 127, 480, 240));   // C1
    music.add_note(music_note(3, 31, 100, 1200, 240));  // G1

    return music;
}

int main() {
    std::cout << "NES Channel Assignment System Test\n";
    std::cout << "===================================\n\n";

    // Create assignment engine
    channel_assignment_engine engine;

    // Test different strategies
    std::vector<std::string> strategies = {"authentic_nes", "modern_chiptune", "quality_focused", "creative_freedom"};

    // Create sample music data
    music_data music = create_sample_music();

    for (const std::string& strategy_name : strategies) {
        std::cout << "Testing Strategy: " << strategy_name << "\n";
        std::cout << std::string(50, '-') << "\n";

        engine.set_active_strategy(strategy_name);

        // Analyze tracks
        std::vector<track_analysis> analyses = engine.analyze_all_tracks(music);

        std::cout << "Track Analysis:\n";
        for (const auto& analysis : analyses) {
            print_track_analysis(analysis);
        }

        // Perform assignment
        assignment_result result = engine.assign_channels(analyses);
        print_assignment_result(result);

        std::cout << "\n";
    }

    // Test individual track analysis
    std::cout << "Individual Track Analysis Example:\n";
    std::cout << std::string(50, '-') << "\n";
    track_analysis melody_analysis = engine.analyze_track(music, 0);
    print_track_analysis(melody_analysis);

    std::cout << "Test completed successfully!\n";
    return 0;
}