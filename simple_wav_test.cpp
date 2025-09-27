#include "src/audio_stream.h"
#include <iostream>
#include <cmath>

int main() {
    try {
        std::cout << "Testing simple WAV file creation..." << std::endl;

        // Create a file writer
        audio_file_writer writer("simple_test.wav", 44100);

        // Generate a simple sine wave tone (440 Hz A4)
        const int duration_samples = 44100; // 1 second
        const double frequency = 440.0;
        const double sample_rate = 44100.0;

        std::vector<int16_t> samples(duration_samples);

        for (int i = 0; i < duration_samples; i++) {
            double t = (double)i / sample_rate;
            double sine_value = sin(2.0 * M_PI * frequency * t);
            samples[i] = (int16_t)(sine_value * 16000); // Scale to 16-bit range
        }

        std::cout << "Generated " << samples.size() << " samples at " << frequency << " Hz" << std::endl;

        // Write samples to WAV file
        if (writer.write_frames(samples.data(), samples.size())) {
            std::cout << "✓ WAV file write successful!" << std::endl;
        } else {
            std::cout << "✗ WAV file write failed!" << std::endl;
            return 1;
        }

        // Close the file
        writer.close();

        std::cout << "✓ WAV file 'simple_test.wav' created successfully!" << std::endl;
        std::cout << "✓ File should contain a 1-second 440Hz sine wave tone" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}