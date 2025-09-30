#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>
#include <cstdio>

// Forward declarations
class audio_device;
class audio_device_manager;

/**
 * Real-time audio streaming system for MAME audio devices
 * Provides low-latency audio output with configurable buffer sizes
 */
class audio_stream {
public:
    using audio_callback_t = std::function<void(int16_t* buffer, size_t frames)>;
    using post_process_callback_t = std::function<void(size_t frames)>;  // Called after buffer processing

    struct config {
        uint32_t sample_rate;
        uint32_t buffer_size;  // frames per buffer
        uint32_t num_buffers;  // number of buffers for triple buffering
        bool enable_threading;

        config() : sample_rate(44100), buffer_size(1024), num_buffers(4), enable_threading(true) {}
    };

    explicit audio_stream(const config& cfg = config{});
    ~audio_stream();

    // Stream lifecycle
    bool initialize();
    bool start();
    bool stop();
    void shutdown();

    // Audio source management
    void set_callback(audio_callback_t callback);
    void set_post_process_callback(post_process_callback_t callback);  // For offline rendering sample counting
    void set_audio_manager(audio_device_manager* manager);

    // File output configuration (must be called before initialize for FILE_OUTPUT backend)
    void set_output_filename(const std::string& filename);
    void force_file_output_mode();

    // Configuration
    uint32_t get_sample_rate() const { return m_config.sample_rate; }
    uint32_t get_buffer_size() const { return m_config.buffer_size; }
    bool is_running() const { return m_running; }

    // Performance metrics
    struct stats {
        uint64_t frames_processed = 0;
        uint64_t buffer_underruns = 0;
        uint64_t buffer_overruns = 0;
        double cpu_usage = 0.0;
    };
    stats get_stats() const;

private:
    config m_config;
    audio_callback_t m_callback;
    post_process_callback_t m_post_process_callback;
    audio_device_manager* m_audio_manager = nullptr;

    // Threading
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_running{false};
    std::unique_ptr<std::thread> m_audio_thread;
    std::mutex m_mutex;
    std::condition_variable m_condition;

    // Audio buffers
    std::vector<std::vector<int16_t>> m_buffers;
    std::atomic<size_t> m_write_buffer{0};
    std::atomic<size_t> m_read_buffer{0};

    // Statistics
    mutable std::mutex m_stats_mutex;
    stats m_stats;

    // Internal methods
    void audio_thread_proc();
    void process_audio_buffer(int16_t* buffer, size_t frames);
    void update_stats();

    // Platform-specific audio output
    bool initialize_platform_audio();
    bool start_platform_audio();
    bool stop_platform_audio();
    void shutdown_platform_audio();

#ifdef __linux__
    // ALSA support
    void* m_alsa_handle = nullptr;
    bool initialize_alsa();
    void shutdown_alsa();
    void alsa_write_frames(const int16_t* buffer, size_t frames);
#endif

#ifdef _WIN32
    // DirectSound support
    void* m_dsound_interface = nullptr;
    void* m_dsound_buffer = nullptr;
    bool initialize_directsound();
    void shutdown_directsound();
    void dsound_write_frames(const int16_t* buffer, size_t frames);
#endif

    // Fallback: File output for testing
    bool m_file_output_mode = false;
    std::string m_output_filename = "audio_output.wav";
    std::unique_ptr<class audio_file_writer> m_file_writer;
    bool initialize_file_output();
    void shutdown_file_output();
    void write_frames_to_file(const int16_t* buffer, size_t frames);
};

/**
 * Simple WAV file writer for testing and debugging
 */
class audio_file_writer {
public:
    explicit audio_file_writer(const std::string& filename, uint32_t sample_rate = 44100);
    ~audio_file_writer();

    bool write_frames(const int16_t* buffer, size_t frames);
    void close();

private:
    std::string m_filename;
    uint32_t m_sample_rate;
    uint64_t m_frames_written = 0;
    std::unique_ptr<FILE, decltype(&fclose)> m_file;

    void write_wav_header();
    void update_wav_header();
};

/**
 * Audio stream factory for creating platform-appropriate streams
 */
class audio_stream_factory {
public:
    enum class backend_type {
        AUTO,       // Auto-detect best available backend
        ALSA,       // Linux ALSA
        DIRECTSOUND, // Windows DirectSound
        FILE_OUTPUT  // File output (for testing)
    };

    static std::unique_ptr<audio_stream> create_stream(
        backend_type backend = backend_type::AUTO,
        const audio_stream::config& config = audio_stream::config{}
    );

    static std::vector<backend_type> get_available_backends();
    static const char* backend_name(backend_type backend);
};
