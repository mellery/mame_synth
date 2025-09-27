#include "test_framework_enhanced.h"
#include "../src/audio_stream.h"
#include <thread>
#include <chrono>

// Unit tests for audio stream functionality
REGISTER_TEST(audio_stream, basic_creation) {
    audio_stream::config config;
    config.sample_rate = 44100;
    config.buffer_size = 1024;
    config.num_buffers = 4;
    config.enable_threading = true;

    audio_stream stream(config);
    ASSERT_EQ(stream.get_sample_rate(), 44100);
    ASSERT_EQ(stream.get_buffer_size(), 1024);
    ASSERT_FALSE(stream.is_running()); // Should not be running yet
}

REGISTER_TEST(audio_stream, buffer_operations) {
    audio_stream::config config;
    config.sample_rate = 44100;
    config.buffer_size = 512;
    config.num_buffers = 4;

    audio_stream stream(config);

    // Test initialization
    ASSERT_TRUE(stream.initialize());

    // Test basic operations (the actual buffer operations are internal)
    ASSERT_EQ(stream.get_sample_rate(), 44100);
    ASSERT_EQ(stream.get_buffer_size(), 512);
}

REGISTER_TEST(audio_stream, lifecycle_management) {
    audio_stream::config config;
    config.sample_rate = 48000;
    config.buffer_size = 256;
    config.num_buffers = 4;

    audio_stream stream(config);

    // Test lifecycle
    ASSERT_TRUE(stream.initialize());
    ASSERT_TRUE(stream.start());
    ASSERT_TRUE(stream.is_running());
    ASSERT_TRUE(stream.stop());
    ASSERT_FALSE(stream.is_running());
    stream.shutdown();
}

REGISTER_TEST(audio_stream, statistics_tracking) {
    audio_stream::config config;
    config.sample_rate = 44100;
    config.buffer_size = 128;
    config.num_buffers = 4;

    audio_stream stream(config);
    ASSERT_TRUE(stream.initialize());

    // Get initial stats
    auto stats = stream.get_stats();
    ASSERT_EQ(stats.frames_processed, 0);
    ASSERT_EQ(stats.buffer_underruns, 0);
    ASSERT_EQ(stats.buffer_overruns, 0);
}

REGISTER_TEST(audio_stream, file_output) {
    audio_stream::config config;
    config.sample_rate = 22050;
    config.buffer_size = 512;
    config.num_buffers = 4;

    audio_stream stream(config);
    stream.set_output_filename("/tmp/test_audio_output.wav");
    ASSERT_TRUE(stream.initialize());

    // Test that we can start/stop
    ASSERT_TRUE(stream.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(stream.stop());

    stream.shutdown();
}

// Performance tests for audio stream
REGISTER_PERFORMANCE_TEST(audio_stream, initialization_speed) {
    static audio_stream::config test_config;
    test_config.sample_rate = 44100;
    test_config.buffer_size = 1024;
    test_config.num_buffers = 4;

    // Test initialization performance
    audio_stream stream(test_config);
    stream.initialize();
}

REGISTER_PERFORMANCE_TEST(audio_stream, callback_performance) {
    static audio_stream::config test_config;
    test_config.sample_rate = 44100;
    test_config.buffer_size = 1024;

    static audio_stream stream(test_config);

    // Test callback setting performance
    stream.set_callback([](int16_t* buffer, size_t frames) {
        // Simple test callback
        for (size_t i = 0; i < frames; ++i) {
            buffer[i] = 0; // Silence
        }
    });
}

// Stress tests for audio stream
REGISTER_STRESS_TEST(audio_stream, rapid_start_stop) {
    static audio_stream::config test_config;
    test_config.sample_rate = 44100;
    test_config.buffer_size = 2048;
    test_config.num_buffers = 4;

    static audio_stream stream(test_config);
    stream.initialize();

    // Rapid start/stop cycles
    for (int i = 0; i < 10; ++i) {
        stream.start();
        stream.stop();
    }
}

REGISTER_STRESS_TEST(audio_stream, high_sample_rate_stress) {
    audio_stream::config test_config;
    test_config.sample_rate = 192000; // High sample rate
    test_config.buffer_size = 64;     // Small buffer for stress
    test_config.num_buffers = 8;

    audio_stream stream(test_config);

    // Should handle high sample rates without issues
    ASSERT_TRUE(stream.initialize());
    ASSERT_EQ(stream.get_sample_rate(), 192000);

    stream.shutdown();
}