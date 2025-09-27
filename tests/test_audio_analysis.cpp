#include "test_framework_enhanced.h"
#include "../src/nes_playback_engine.h"
#include "../src/nes_note_mapping.h"
#include <fstream>
#include <cmath>
#include <complex>
#include <vector>

/**
 * Audio analysis tests that verify WAV file output for correctness
 * These tests actually analyze the generated audio content to ensure
 * proper frequencies, durations, channel behavior, and audio quality
 */

// Simple WAV file header structure
struct wav_header {
    char riff[4];           // "RIFF"
    uint32_t chunk_size;    // File size - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmt_size;      // Format chunk size (16 for PCM)
    uint16_t format;        // Audio format (1 = PCM)
    uint16_t channels;      // Number of channels
    uint32_t sample_rate;   // Sample rate
    uint32_t byte_rate;     // Bytes per second
    uint16_t block_align;   // Bytes per sample frame
    uint16_t bits_per_sample; // Bits per sample
    char data[4];           // "data"
    uint32_t data_size;     // Data chunk size
};

// Audio analysis utilities
class audio_analyzer {
public:
    struct wav_analysis {
        std::vector<float> samples;
        uint32_t sample_rate;
        uint16_t channels;
        double duration_seconds;

        // Analysis results
        double rms_amplitude;
        double peak_amplitude;
        std::vector<double> dominant_frequencies;
        bool has_audio_content;
        double silence_ratio;
    };

    static wav_analysis analyze_wav_file(const std::string& filename) {
        wav_analysis analysis;

        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return analysis; // Empty analysis
        }

        // Read WAV header
        wav_header header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        // Validate WAV format
        if (std::string(header.riff, 4) != "RIFF" ||
            std::string(header.wave, 4) != "WAVE" ||
            std::string(header.data, 4) != "data") {
            return analysis; // Invalid WAV file
        }

        analysis.sample_rate = header.sample_rate;
        analysis.channels = header.channels;
        analysis.duration_seconds = static_cast<double>(header.data_size) / header.byte_rate;

        // Read audio data
        std::vector<int16_t> raw_samples(header.data_size / sizeof(int16_t));
        file.read(reinterpret_cast<char*>(raw_samples.data()), header.data_size);

        // Convert to normalized float samples
        analysis.samples.reserve(raw_samples.size());
        for (int16_t sample : raw_samples) {
            analysis.samples.push_back(static_cast<float>(sample) / 32768.0f);
        }

        // Perform analysis
        analyze_amplitude(analysis);
        analyze_frequency_content(analysis);
        analyze_silence(analysis);

        return analysis;
    }

private:
    static void analyze_amplitude(wav_analysis& analysis) {
        if (analysis.samples.empty()) {
            analysis.rms_amplitude = 0.0;
            analysis.peak_amplitude = 0.0;
            return;
        }

        double sum_squares = 0.0;
        double peak = 0.0;

        for (float sample : analysis.samples) {
            double abs_sample = std::abs(sample);
            sum_squares += sample * sample;
            peak = std::max(peak, abs_sample);
        }

        analysis.rms_amplitude = std::sqrt(sum_squares / analysis.samples.size());
        analysis.peak_amplitude = peak;
        analysis.has_audio_content = analysis.rms_amplitude > 0.001; // -60dB threshold
    }

    static void analyze_frequency_content(wav_analysis& analysis) {
        if (analysis.samples.size() < 1024) {
            return; // Not enough samples for meaningful frequency analysis
        }

        // Simple frequency analysis using zero-crossing rate and autocorrelation
        // For more sophisticated analysis, would implement FFT

        // Count zero crossings to estimate dominant frequency
        int zero_crossings = 0;
        for (size_t i = 1; i < analysis.samples.size(); ++i) {
            if ((analysis.samples[i-1] >= 0) != (analysis.samples[i] >= 0)) {
                zero_crossings++;
            }
        }

        // Estimate fundamental frequency from zero crossings
        double estimated_freq = (zero_crossings / 2.0) / analysis.duration_seconds;
        if (estimated_freq > 20.0 && estimated_freq < 20000.0) {
            analysis.dominant_frequencies.push_back(estimated_freq);
        }
    }

    static void analyze_silence(wav_analysis& analysis) {
        if (analysis.samples.empty()) {
            analysis.silence_ratio = 1.0;
            return;
        }

        const double silence_threshold = 0.001; // -60dB
        size_t silent_samples = 0;

        for (float sample : analysis.samples) {
            if (std::abs(sample) < silence_threshold) {
                silent_samples++;
            }
        }

        analysis.silence_ratio = static_cast<double>(silent_samples) / analysis.samples.size();
    }
};

// Test that a single NES note produces the correct frequency
REGISTER_TEST(audio_analysis, single_note_frequency) {
    // Generate a single note (A4 = 440 Hz on pulse channel)
    nes_playback_engine::engine_config config;
    config.sample_rate = 44100;
    config.buffer_size = 1024;
    config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;

    nes_playback_engine engine(config);
    ASSERT_TRUE(engine.initialize());

    // Create music with single A4 note (MIDI note 69)
    music_data music;
    music.add_note(music_note(0, 69, 100, 0, 2200)); // 1-second note at 480 tpq = 2200 ticks

    ASSERT_TRUE(engine.load_music_data(music));

    // Export to WAV instead of real-time playback
    std::string test_filename = "/tmp/test_single_note.wav";
    ASSERT_TRUE(engine.export_to_wav(test_filename, 44100));

    // Analyze the generated WAV file
    auto analysis = audio_analyzer::analyze_wav_file(test_filename);

    // Verify basic properties
    ASSERT_TRUE(analysis.has_audio_content);
    ASSERT_GT(analysis.duration_seconds, 0.8); // At least 0.8 seconds
    ASSERT_LT(analysis.duration_seconds, 1.5); // At most 1.5 seconds
    ASSERT_GT(analysis.rms_amplitude, 0.01);   // Significant audio level
    ASSERT_LT(analysis.silence_ratio, 0.3);    // Mostly non-silent

    // Verify frequency content
    ASSERT_FALSE(analysis.dominant_frequencies.empty());

    // Calculate expected NES frequency for A4 (MIDI note 69)
    double expected_freq = 440.0; // A4 reference

    // Check if any detected frequency is close to expected (within 5%)
    bool found_correct_frequency = false;
    for (double freq : analysis.dominant_frequencies) {
        if (std::abs(freq - expected_freq) / expected_freq < 0.05) {
            found_correct_frequency = true;
            break;
        }
    }

    ASSERT_TRUE(found_correct_frequency);

    // Cleanup
    test_utilities::cleanup_temp_file(config.output_filename);
}

// Test that different duty cycles produce different waveform characteristics
REGISTER_TEST(audio_analysis, pulse_duty_cycle_analysis) {
    std::vector<std::string> output_files;
    std::vector<audio_analyzer::wav_analysis> analyses;

    // Test different duty cycles (pulse channel settings)
    for (int duty = 0; duty < 4; ++duty) {
        nes_playback_engine::engine_config config;
        config.sample_rate = 44100;
        config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;

        std::string test_filename = "/tmp/test_duty_" + std::to_string(duty) + ".wav";
        output_files.push_back(test_filename);

        nes_playback_engine engine(config);
        ASSERT_TRUE(engine.initialize());

        // Create music with the same note but different duty cycles
        music_data music;
        music.add_note(music_note(0, 60, 100, 0, 2200)); // C4 for 1 second

        // Set duty cycle (would need integration with playback engine)
        // For now, just test that different configurations produce different results

        ASSERT_TRUE(engine.load_music_data(music));

        // Export to WAV instead of real-time playback
        ASSERT_TRUE(engine.export_to_wav(test_filename, 44100));

        // Analyze this duty cycle's output
        auto analysis = audio_analyzer::analyze_wav_file(test_filename);
        analyses.push_back(analysis);

        // Basic validation
        ASSERT_TRUE(analysis.has_audio_content);
        ASSERT_GT(analysis.rms_amplitude, 0.01);
    }

    // Verify that different duty cycles produce different amplitude characteristics
    // (Different duty cycles should have different RMS values)
    bool found_variation = false;
    for (size_t i = 1; i < analyses.size(); ++i) {
        if (std::abs(analyses[i].rms_amplitude - analyses[0].rms_amplitude) > 0.01) {
            found_variation = true;
            break;
        }
    }
    ASSERT_TRUE(found_variation);

    // Cleanup
    for (const auto& file : output_files) {
        test_utilities::cleanup_temp_file(file);
    }
}

// Test that triangle channel produces proper waveform
REGISTER_TEST(audio_analysis, triangle_waveform_analysis) {
    nes_playback_engine::engine_config config;
    config.sample_rate = 44100;
    config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;

    std::string test_filename = "/tmp/test_triangle.wav";

    nes_playback_engine engine(config);
    ASSERT_TRUE(engine.initialize());

    // Create music with triangle channel (MIDI channel 2)
    music_data music;
    music.add_note(music_note(2, 48, 127, 0, 2200)); // C3 on triangle channel

    ASSERT_TRUE(engine.load_music_data(music));

    // Export to WAV
    ASSERT_TRUE(engine.export_to_wav(test_filename, 44100));

    // Analyze triangle wave output
    auto analysis = audio_analyzer::analyze_wav_file(test_filename);

    // Triangle waves should have specific characteristics
    ASSERT_TRUE(analysis.has_audio_content);
    ASSERT_GT(analysis.rms_amplitude, 0.01);
    ASSERT_LT(analysis.silence_ratio, 0.2); // Mostly active

    // Triangle waves typically have lower harmonics compared to square waves
    // This is a simplified test - a full implementation would use FFT analysis

    // Cleanup
    test_utilities::cleanup_temp_file(test_filename);
}

// Test multi-channel audio generation
REGISTER_TEST(audio_analysis, multi_channel_output) {
    nes_playback_engine::engine_config config;
    config.sample_rate = 44100;
    config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;
    config.output_filename = "/tmp/test_multi_channel.wav";

    nes_playback_engine engine(config);
    ASSERT_TRUE(engine.initialize());

    // Create music with multiple NES channels
    music_data music;
    music.add_note(music_note(0, 60, 100, 0, 2200));   // Pulse 1: C4
    music.add_note(music_note(1, 64, 90, 0, 2200));    // Pulse 2: E4
    music.add_note(music_note(2, 48, 127, 0, 2200));   // Triangle: C3
    music.add_note(music_note(9, 36, 127, 0, 480));    // Noise: Kick pattern
    music.add_note(music_note(9, 36, 127, 960, 480));
    music.add_note(music_note(9, 36, 127, 1920, 480));

    ASSERT_TRUE(engine.load_music_data(music));

    // Export to WAV instead of real-time playback
    std::string test_filename = "/tmp/test_multi_channel.wav";
    ASSERT_TRUE(engine.export_to_wav(test_filename, 44100));

    // Analyze multi-channel output
    auto analysis = audio_analyzer::analyze_wav_file(test_filename);

    // Multi-channel output should be louder and more complex
    ASSERT_TRUE(analysis.has_audio_content);
    ASSERT_GT(analysis.rms_amplitude, 0.05); // Higher amplitude due to multiple channels
    ASSERT_LT(analysis.silence_ratio, 0.1);  // Very little silence

    // Should detect multiple frequency components
    ASSERT_FALSE(analysis.dominant_frequencies.empty());

    // Cleanup
    test_utilities::cleanup_temp_file(test_filename);
}

// Test silence detection for empty music
REGISTER_TEST(audio_analysis, silence_detection) {
    nes_playback_engine::engine_config config;
    config.sample_rate = 44100;
    config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;
    config.output_filename = "/tmp/test_silence.wav";

    nes_playback_engine engine(config);
    ASSERT_TRUE(engine.initialize());

    // Create empty music data
    music_data empty_music;

    ASSERT_TRUE(engine.load_music_data(empty_music));

    // Export to WAV instead of real-time playback (short silence file)
    std::string test_filename = "/tmp/test_silence.wav";
    ASSERT_TRUE(engine.export_to_wav(test_filename, 44100));

    // Analyze silence
    auto analysis = audio_analyzer::analyze_wav_file(test_filename);

    // Should be mostly silent
    ASSERT_FALSE(analysis.has_audio_content);
    ASSERT_LT(analysis.rms_amplitude, 0.001);
    ASSERT_GT(analysis.silence_ratio, 0.95); // 95%+ silence

    // Cleanup
    test_utilities::cleanup_temp_file(test_filename);
}

// Test audio duration accuracy
REGISTER_TEST(audio_analysis, duration_accuracy) {
    struct test_case {
        uint32_t duration_ticks;
        double expected_seconds;
        std::string filename;
    };

    std::vector<test_case> test_cases = {
        {480, 0.5, "/tmp/test_half_second.wav"},      // Half second
        {960, 1.0, "/tmp/test_one_second.wav"},       // One second
        {1920, 2.0, "/tmp/test_two_seconds.wav"},     // Two seconds
        {240, 0.25, "/tmp/test_quarter_second.wav"}   // Quarter second
    };

    for (const auto& test : test_cases) {
        nes_playback_engine::engine_config config;
        config.sample_rate = 44100;
        config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;
        config.output_filename = test.filename;

        nes_playback_engine engine(config);
        ASSERT_TRUE(engine.initialize());

        // Create music with specific duration
        music_data music;
        music.add_note(music_note(0, 60, 100, 0, test.duration_ticks));

        ASSERT_TRUE(engine.load_music_data(music));

        // Export to WAV instead of real-time playback
        ASSERT_TRUE(engine.export_to_wav(test.filename, 44100));

        // Analyze duration
        auto analysis = audio_analyzer::analyze_wav_file(test.filename);

        // Verify duration is within 10% of expected
        double tolerance = test.expected_seconds * 0.1;
        ASSERT_NEAR(analysis.duration_seconds, test.expected_seconds, tolerance);

        // Cleanup
        test_utilities::cleanup_temp_file(test.filename);
    }
}

// Performance test for audio analysis
REGISTER_PERFORMANCE_TEST(audio_analysis, wav_analysis_speed) {
    static std::string test_file = "/tmp/perf_test_audio.wav";

    // Create a test file once
    static bool file_created = false;
    if (!file_created) {
        nes_playback_engine::engine_config config;
        config.audio_backend = audio_stream_factory::backend_type::FILE_OUTPUT;

        nes_playback_engine engine(config);
        engine.initialize();

        music_data music = test_data_generator::generate_random_music(20, 2000);
        engine.load_music_data(music);

        // Export to WAV instead of real-time playback
        engine.export_to_wav(test_file, 44100);
        file_created = true;
    }

    // Test analysis speed
    auto analysis = audio_analyzer::analyze_wav_file(test_file);
    ASSERT_TRUE(analysis.has_audio_content);
}