#include "test_framework.h"
#include "../src/audio_stream.h"
#include "../src/debug_config.h"
#include <fstream>
#include <cmath>
#include <iostream>
#include <cstring>

namespace test_wav_header_integrity {

// Helper to read WAV header
struct wav_header {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // File size - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmt_size;      // 16 for PCM
    uint16_t audio_format;  // 1 for PCM
    uint16_t num_channels;  // 1 for mono
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];           // "data"
    uint32_t data_size;
};

bool read_wav_header(const std::string& filename, wav_header& header) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "Failed to open file: " << filename << std::endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(&header), sizeof(wav_header));
    return file.good();
}

void print_wav_header(const wav_header& header) {
    std::cout << "WAV Header Analysis:" << std::endl;
    std::cout << "  RIFF: " << std::string(header.riff, 4) << std::endl;
    std::cout << "  File size: " << header.file_size << " bytes" << std::endl;
    std::cout << "  WAVE: " << std::string(header.wave, 4) << std::endl;
    std::cout << "  Format: " << std::string(header.fmt, 4) << std::endl;
    std::cout << "  Audio format: " << header.audio_format << std::endl;
    std::cout << "  Channels: " << header.num_channels << std::endl;
    std::cout << "  Sample rate: " << header.sample_rate << " Hz" << std::endl;
    std::cout << "  Byte rate: " << header.byte_rate << std::endl;
    std::cout << "  Block align: " << header.block_align << std::endl;
    std::cout << "  Bits per sample: " << header.bits_per_sample << std::endl;
    std::cout << "  DATA: " << std::string(header.data, 4) << std::endl;
    std::cout << "  Data size: " << header.data_size << " bytes" << std::endl;

    // Calculate expected values
    double duration_sec = static_cast<double>(header.data_size) /
                         (header.sample_rate * header.num_channels * (header.bits_per_sample / 8));
    std::cout << "  Calculated duration: " << duration_sec << " seconds" << std::endl;
}

bool verify_wav_header(const wav_header& header, uint32_t expected_sample_rate,
                       size_t expected_frames) {
    bool valid = true;

    // Check magic numbers
    if (std::strncmp(header.riff, "RIFF", 4) != 0) {
        std::cout << "ERROR: Invalid RIFF magic" << std::endl;
        valid = false;
    }

    if (std::strncmp(header.wave, "WAVE", 4) != 0) {
        std::cout << "ERROR: Invalid WAVE magic" << std::endl;
        valid = false;
    }

    if (std::strncmp(header.data, "data", 4) != 0) {
        std::cout << "ERROR: Invalid data magic" << std::endl;
        valid = false;
    }

    // Check format
    if (header.audio_format != 1) {
        std::cout << "ERROR: Audio format is not PCM (expected 1, got "
                  << header.audio_format << ")" << std::endl;
        valid = false;
    }

    if (header.num_channels != 1) {
        std::cout << "ERROR: Expected mono (1 channel), got "
                  << header.num_channels << std::endl;
        valid = false;
    }

    if (header.bits_per_sample != 16) {
        std::cout << "ERROR: Expected 16 bits per sample, got "
                  << header.bits_per_sample << std::endl;
        valid = false;
    }

    // Check sample rate
    if (header.sample_rate != expected_sample_rate) {
        std::cout << "ERROR: Sample rate mismatch (expected "
                  << expected_sample_rate << ", got " << header.sample_rate << ")" << std::endl;
        valid = false;
    }

    // Check data size
    uint32_t expected_data_size = expected_frames * sizeof(int16_t);
    if (header.data_size != expected_data_size) {
        std::cout << "ERROR: Data size mismatch (expected "
                  << expected_data_size << ", got " << header.data_size << ")" << std::endl;
        std::cout << "  Expected frames: " << expected_frames << std::endl;
        std::cout << "  Actual frames: " << (header.data_size / sizeof(int16_t)) << std::endl;
        valid = false;
    }

    // Check file size
    uint32_t expected_file_size = header.data_size + 36;
    if (header.file_size != expected_file_size) {
        std::cout << "ERROR: File size mismatch (expected "
                  << expected_file_size << ", got " << header.file_size << ")" << std::endl;
        valid = false;
    }

    // Check byte rate
    uint32_t expected_byte_rate = header.sample_rate * header.num_channels * (header.bits_per_sample / 8);
    if (header.byte_rate != expected_byte_rate) {
        std::cout << "ERROR: Byte rate mismatch (expected "
                  << expected_byte_rate << ", got " << header.byte_rate << ")" << std::endl;
        valid = false;
    }

    // Check if sizes are zero (the main issue we're debugging)
    if (header.file_size == 0) {
        std::cout << "CRITICAL: File size field is zero!" << std::endl;
        valid = false;
    }

    if (header.data_size == 0) {
        std::cout << "CRITICAL: Data size field is zero!" << std::endl;
        valid = false;
    }

    return valid;
}

// Test 1: Simple sine wave generation
bool test_simple_sine_wave() {
    std::cout << "\n=== Test: Simple Sine Wave (5 seconds) ===" << std::endl;

    // Enable debug logging
    g_debug_config.log_wav_export = true;
    g_debug_config.log_file_operations = true;

    const std::string filename = "/tmp/test_sine_wave.wav";
    const uint32_t sample_rate = 48000;
    const double duration_sec = 5.0;
    const size_t num_frames = static_cast<size_t>(duration_sec * sample_rate);
    const double frequency = 440.0; // A4

    std::cout << "Generating " << duration_sec << "s sine wave at " << frequency << "Hz" << std::endl;
    std::cout << "Expected frames: " << num_frames << std::endl;

    // Create audio file writer
    audio_file_writer writer(filename, sample_rate);

    // Generate and write sine wave
    const size_t buffer_size = 1024;
    int16_t buffer[buffer_size];
    size_t frames_written = 0;

    for (size_t i = 0; i < num_frames; i += buffer_size) {
        size_t frames_to_write = std::min(buffer_size, num_frames - i);

        for (size_t j = 0; j < frames_to_write; ++j) {
            double t = static_cast<double>(i + j) / sample_rate;
            double sample = std::sin(2.0 * M_PI * frequency * t) * 16000.0;
            buffer[j] = static_cast<int16_t>(sample);
        }

        writer.write_frames(buffer, frames_to_write);
        frames_written += frames_to_write;
    }

    std::cout << "Total frames written: " << frames_written << std::endl;

    // Close file (this should update the header)
    writer.close();

    // Read and verify header
    wav_header header;
    if (!read_wav_header(filename, header)) {
        std::cout << "FAIL: Could not read WAV header" << std::endl;
        return false;
    }

    print_wav_header(header);

    bool valid = verify_wav_header(header, sample_rate, frames_written);

    std::cout << (valid ? "PASS: WAV header is valid" : "FAIL: WAV header is invalid") << std::endl;
    return valid;
}

// Test 2: Very short file (edge case)
bool test_short_file() {
    std::cout << "\n=== Test: Very Short File (0.1 seconds) ===" << std::endl;

    const std::string filename = "/tmp/test_short.wav";
    const uint32_t sample_rate = 48000;
    const double duration_sec = 0.1;
    const size_t num_frames = static_cast<size_t>(duration_sec * sample_rate);

    audio_file_writer writer(filename, sample_rate);

    int16_t buffer[4800];
    for (size_t i = 0; i < num_frames; ++i) {
        buffer[i] = static_cast<int16_t>(i * 100);
    }

    writer.write_frames(buffer, num_frames);
    writer.close();

    wav_header header;
    if (!read_wav_header(filename, header)) {
        return false;
    }

    print_wav_header(header);
    return verify_wav_header(header, sample_rate, num_frames);
}

// Test 3: Silence (all zeros)
bool test_silence() {
    std::cout << "\n=== Test: Silence (1 second) ===" << std::endl;

    const std::string filename = "/tmp/test_silence.wav";
    const uint32_t sample_rate = 48000;
    const size_t num_frames = sample_rate;

    audio_file_writer writer(filename, sample_rate);

    int16_t buffer[48000];
    std::memset(buffer, 0, sizeof(buffer));

    writer.write_frames(buffer, num_frames);
    writer.close();

    wav_header header;
    if (!read_wav_header(filename, header)) {
        return false;
    }

    print_wav_header(header);
    return verify_wav_header(header, sample_rate, num_frames);
}

// Test 4: Multiple write calls
bool test_multiple_writes() {
    std::cout << "\n=== Test: Multiple Write Calls (10 x 0.1s) ===" << std::endl;

    const std::string filename = "/tmp/test_multiple_writes.wav";
    const uint32_t sample_rate = 48000;
    const size_t frames_per_write = 4800; // 0.1 seconds
    const size_t num_writes = 10;
    const size_t total_frames = frames_per_write * num_writes;

    audio_file_writer writer(filename, sample_rate);

    int16_t buffer[4800];
    for (size_t write = 0; write < num_writes; ++write) {
        for (size_t i = 0; i < frames_per_write; ++i) {
            buffer[i] = static_cast<int16_t>((write * 1000) + i);
        }
        writer.write_frames(buffer, frames_per_write);
    }

    writer.close();

    wav_header header;
    if (!read_wav_header(filename, header)) {
        return false;
    }

    print_wav_header(header);
    return verify_wav_header(header, sample_rate, total_frames);
}

void run_all_tests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "WAV Header Integrity Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    int passed = 0;
    int total = 0;

    total++;
    if (test_simple_sine_wave()) passed++;

    total++;
    if (test_short_file()) passed++;

    total++;
    if (test_silence()) passed++;

    total++;
    if (test_multiple_writes()) passed++;

    std::cout << "\n========================================" << std::endl;
    std::cout << "Results: " << passed << "/" << total << " tests passed" << std::endl;
    std::cout << "========================================" << std::endl;
}

} // namespace test_wav_header_integrity

// Export function for test framework
extern "C" void run_wav_header_integrity_tests() {
    test_wav_header_integrity::run_all_tests();
}