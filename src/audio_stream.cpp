#include "audio_stream.h"
#include "audio_device.h"
#include "debug_config.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <future>

// Platform detection
#ifdef __linux__
#include <alsa/asoundlib.h>
#define HAVE_ALSA
#endif

#ifdef _WIN32
#include <windows.h>
#include <dsound.h>
#define HAVE_DIRECTSOUND
#endif

audio_stream::audio_stream(const config& cfg)
    : m_config(cfg) {
    // Initialize buffers
    m_buffers.resize(m_config.num_buffers);
    for (auto& buffer : m_buffers) {
        buffer.resize(m_config.buffer_size);
    }
}

audio_stream::~audio_stream() {
    shutdown();
}

bool audio_stream::initialize() {
    if (m_initialized) {
        return true;
    }

    std::cout << "Initializing audio stream:" << std::endl;
    std::cout << "  Sample rate: " << m_config.sample_rate << "Hz" << std::endl;
    std::cout << "  Buffer size: " << m_config.buffer_size << " frames" << std::endl;
    std::cout << "  Num buffers: " << m_config.num_buffers << std::endl;

    // Check if file output mode was explicitly requested
    if (m_file_output_mode) {
        std::cout << "Using file output mode (explicitly requested)" << std::endl;
        if (!initialize_file_output()) {
            std::cout << "Failed to initialize file output" << std::endl;
            return false;
        }
    } else {
        // Try platform-specific audio first, fall back to file output
        if (!initialize_platform_audio()) {
            std::cout << "Platform audio failed, using file output mode" << std::endl;
            m_file_output_mode = true;
            if (!initialize_file_output()) {
                std::cout << "Failed to initialize file output" << std::endl;
                return false;
            }
        }
    }

    m_initialized = true;
    std::cout << "Audio stream initialized successfully" << std::endl;
    return true;
}

bool audio_stream::start() {
    if (!m_initialized || m_running) {
        return false;
    }

    std::cout << "Starting audio stream..." << std::endl;

    if (!m_file_output_mode && !start_platform_audio()) {
        std::cout << "Failed to start platform audio" << std::endl;
        return false;
    }

    m_running = true;

    if (m_config.enable_threading) {
        m_audio_thread = std::make_unique<std::thread>(&audio_stream::audio_thread_proc, this);
        std::cout << "Audio thread started" << std::endl;
    }

    std::cout << "Audio stream started successfully" << std::endl;
    return true;
}

bool audio_stream::stop() {
    if (!m_running) {
        return true;
    }

    std::cout << "Stopping audio stream..." << std::endl;
    m_running = false;

    if (m_audio_thread && m_audio_thread->joinable()) {
        std::cout << "Stopping audio thread..." << std::endl;

        // Wake up the audio thread
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_condition.notify_all();
        }

        std::cout << "Waiting for audio thread to join..." << std::endl;

        // Try to join with timeout to avoid infinite hang
        auto future = std::async(std::launch::async, [this]() {
            m_audio_thread->join();
        });

        if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
            std::cout << "Warning: Audio thread did not stop within timeout, detaching..." << std::endl;
            m_audio_thread->detach();
        }

        m_audio_thread.reset();
        std::cout << "Audio thread stopped" << std::endl;
    }

    if (!m_file_output_mode) {
        stop_platform_audio();
    }

    std::cout << "Audio stream stopped" << std::endl;
    return true;
}

void audio_stream::shutdown() {
    stop();

    if (m_initialized) {
        if (m_file_output_mode) {
            shutdown_file_output();
        } else {
            shutdown_platform_audio();
        }
        m_initialized = false;
        std::cout << "Audio stream shut down" << std::endl;
    }
}

void audio_stream::set_callback(audio_callback_t callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = callback;
}

void audio_stream::set_post_process_callback(post_process_callback_t callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_post_process_callback = callback;
}

void audio_stream::set_audio_manager(audio_device_manager* manager) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_audio_manager = manager;
}

void audio_stream::set_output_filename(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_output_filename = filename;
}

void audio_stream::force_file_output_mode() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_file_output_mode = true;
}

audio_stream::stats audio_stream::get_stats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

void audio_stream::audio_thread_proc() {
    std::cout << "Audio thread started (buffer size: " << m_config.buffer_size << " frames)" << std::endl;
    std::cout << "DEBUG: m_file_output_mode=" << m_file_output_mode << std::endl;

    auto next_time = std::chrono::steady_clock::now();
    const auto buffer_duration = std::chrono::microseconds(
        (m_config.buffer_size * 1000000) / m_config.sample_rate);

    int iteration = 0;
    while (m_running) {
        auto start_time = std::chrono::steady_clock::now();

        // Check exit condition early
        if (!m_running) break;

        // Get current write buffer
        size_t write_idx = m_write_buffer.load();
        auto& buffer = m_buffers[write_idx];

        // Process audio
        process_audio_buffer(buffer.data(), m_config.buffer_size);

        // Call post-process callback AFTER generating audio (for offline rendering)
        if (m_post_process_callback) {
            m_post_process_callback(m_config.buffer_size);
        }


        // Check exit condition after processing
        if (!m_running) break;

        if (m_file_output_mode) {
            // Write to file
            if (iteration < 20) {
                std::cout << "DEBUG: iteration " << iteration << ", writing to file" << std::endl;
            }
            write_frames_to_file(buffer.data(), m_config.buffer_size);
        } else {
            if (iteration < 20) {
                std::cout << "DEBUG: iteration " << iteration << ", NOT file mode" << std::endl;
            }
            // Write to audio device
#ifdef HAVE_ALSA
            if (m_alsa_handle) {
                alsa_write_frames(buffer.data(), m_config.buffer_size);
            }
#endif
#ifdef HAVE_DIRECTSOUND
            if (m_dsound_buffer) {
                dsound_write_frames(buffer.data(), m_config.buffer_size);
            }
#endif
        }

        // Update buffer index
        m_write_buffer = (write_idx + 1) % m_config.num_buffers;

        // Update statistics
        update_stats();

        iteration++;

        // Timing: real-time audio needs precise sleep, file output can run fast
        if (!m_file_output_mode) {
            // Real-time audio: sleep to maintain timing
            next_time += buffer_duration;
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait_until(lock, next_time, [this] { return !m_running; });
        } else {
            // File output: sample counting handles timing, run as fast as possible
            // Just check for stop signal with minimal delay
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_condition.wait_for(lock, std::chrono::microseconds(10), [this] { return !m_running; })) {
                break;
            }
        }
    }

    std::cout << "Audio thread finished" << std::endl;
}

void audio_stream::process_audio_buffer(int16_t* buffer, size_t frames) {
    // Clear buffer first
    std::memset(buffer, 0, frames * sizeof(int16_t));

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_callback) {
            // Use custom callback
            m_callback(buffer, frames);
        } else if (m_audio_manager) {
            // Use audio device manager
            m_audio_manager->generate_mixed_samples(buffer, frames);
        }
    }
}

void audio_stream::update_stats() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_stats.frames_processed += m_config.buffer_size;
}

// Platform-specific implementations

bool audio_stream::initialize_platform_audio() {
#ifdef HAVE_ALSA
    return initialize_alsa();
#elif defined(HAVE_DIRECTSOUND)
    return initialize_directsound();
#else
    return false; // No platform audio available
#endif
}

bool audio_stream::start_platform_audio() {
    // Most platform audio starts immediately after initialization
    return true;
}

bool audio_stream::stop_platform_audio() {
    return true;
}

void audio_stream::shutdown_platform_audio() {
#ifdef HAVE_ALSA
    shutdown_alsa();
#elif defined(HAVE_DIRECTSOUND)
    shutdown_directsound();
#endif
}

#ifdef HAVE_ALSA
bool audio_stream::initialize_alsa() {
    snd_pcm_t* handle;
    int err;

    // Open PCM device for playback
    err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        std::cout << "ALSA: Cannot open audio device: " << snd_strerror(err) << std::endl;
        return false;
    }

    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(handle, hw_params);

    // Set hardware parameters
    snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, hw_params, 1); // Mono

    unsigned int rate = m_config.sample_rate;
    snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, 0);

    snd_pcm_uframes_t buffer_size = m_config.buffer_size * 4; // Larger ALSA buffer
    snd_pcm_hw_params_set_buffer_size_near(handle, hw_params, &buffer_size);

    err = snd_pcm_hw_params(handle, hw_params);
    if (err < 0) {
        std::cout << "ALSA: Cannot set hardware parameters: " << snd_strerror(err) << std::endl;
        snd_pcm_close(handle);
        return false;
    }

    // Prepare the device
    err = snd_pcm_prepare(handle);
    if (err < 0) {
        std::cout << "ALSA: Cannot prepare audio interface: " << snd_strerror(err) << std::endl;
        snd_pcm_close(handle);
        return false;
    }

    m_alsa_handle = handle;
    std::cout << "ALSA initialized successfully (rate: " << rate << "Hz, buffer: " << buffer_size << " frames)" << std::endl;
    return true;
}

void audio_stream::shutdown_alsa() {
    if (m_alsa_handle) {
        snd_pcm_drain(static_cast<snd_pcm_t*>(m_alsa_handle));
        snd_pcm_close(static_cast<snd_pcm_t*>(m_alsa_handle));
        m_alsa_handle = nullptr;
        std::cout << "ALSA shut down" << std::endl;
    }
}

void audio_stream::alsa_write_frames(const int16_t* buffer, size_t frames) {
    snd_pcm_t* handle = static_cast<snd_pcm_t*>(m_alsa_handle);
    snd_pcm_sframes_t result = snd_pcm_writei(handle, buffer, frames);

    if (result == -EPIPE) {
        // Buffer underrun
        snd_pcm_prepare(handle);
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        m_stats.buffer_underruns++;
    } else if (result < 0) {
        std::cout << "ALSA write error: " << snd_strerror(result) << std::endl;
    }
}
#endif

// File output implementation
bool audio_stream::initialize_file_output() {
    m_file_writer = std::make_unique<audio_file_writer>(m_output_filename, m_config.sample_rate);
    return true;
}

void audio_stream::shutdown_file_output() {
    if (m_file_writer) {
        m_file_writer->close();
        m_file_writer.reset();
        std::cout << "Audio output written to " << m_output_filename << std::endl;
    }
}

void audio_stream::write_frames_to_file(const int16_t* buffer, size_t frames) {
    if (m_file_writer) {
        if (g_debug_config.log_audio_buffers) {
            // Check if buffer contains non-silent audio
            bool has_audio = false;
            for (size_t i = 0; i < frames && !has_audio; ++i) {
                if (buffer[i] != 0) has_audio = true;
            }

            static size_t total_frames = 0;
            static size_t non_silent_frames = 0;
            total_frames += frames;
            if (has_audio) non_silent_frames += frames;

            if (total_frames % 48000 == 0) { // Log every second at 48kHz
                std::stringstream ss;
                ss << "WAV write: total=" << total_frames
                   << ", non_silent=" << non_silent_frames
                   << " (" << (100.0 * non_silent_frames / total_frames) << "%)";
                DEBUG_LOG_AUDIO(ss.str());
            }
        }
        m_file_writer->write_frames(buffer, frames);
    }
}

// WAV file writer implementation
audio_file_writer::audio_file_writer(const std::string& filename, uint32_t sample_rate)
    : m_filename(filename), m_sample_rate(sample_rate), m_file(nullptr, &fclose) {

    FILE* file = std::fopen(filename.c_str(), "wb");
    if (!file) {
        std::cout << "Failed to create audio file: " << filename << std::endl;
        return;
    }

    m_file.reset(file);
    write_wav_header();
}

audio_file_writer::~audio_file_writer() {
    close();
}

bool audio_file_writer::write_frames(const int16_t* buffer, size_t frames) {
    if (!m_file) return false;

    // DEBUG: Log file positions for first 20 writes
    static int write_count = 0;
    if (write_count < 20) {
        long pos = std::ftell(m_file.get());
        int16_t max_val = 0;
        for (size_t i = 0; i < frames; i++) {
            if (abs(buffer[i]) > abs(max_val)) max_val = buffer[i];
        }
        std::cout << "write[" << write_count << "]: pos=" << pos
                  << ", frames=" << frames << ", max=" << max_val << std::endl;
        write_count++;
    }

    if (g_debug_config.log_file_operations) {
        long pos_before = std::ftell(m_file.get());
        std::stringstream ss;
        ss << "Writing " << frames << " frames at file position " << pos_before;
        DEBUG_LOG_INFO("WAV_IO", ss.str());
    }

    size_t written = std::fwrite(buffer, sizeof(int16_t), frames, m_file.get());

    if (g_debug_config.log_file_operations) {
        long pos_after = std::ftell(m_file.get());
        std::stringstream ss;
        ss << "Wrote " << written << " frames, position now " << pos_after
           << ", total frames: " << (m_frames_written + written);
        DEBUG_LOG_INFO("WAV_IO", ss.str());
    }

    if (written == frames) {
        m_frames_written += frames;
        return true;
    }
    return false;
}

void audio_file_writer::close() {
    if (m_file) {
        update_wav_header();
        m_file.reset();
        std::cout << "Wrote " << m_frames_written << " audio frames to " << m_filename << std::endl;
    }
}

void audio_file_writer::write_wav_header() {
    if (!m_file) return;

    long pos_before = std::ftell(m_file.get());

    if (g_debug_config.log_wav_export) {
        std::stringstream ss;
        ss << "Writing initial WAV header: file=" << m_filename
           << ", sample_rate=" << m_sample_rate
           << ", file_pos=" << pos_before;
        DEBUG_LOG_INFO("WAV", ss.str());
    }

    // WAV header (will be updated later with correct sizes)
    uint8_t header[44] = {
        'R', 'I', 'F', 'F',  // ChunkID
        0, 0, 0, 0,          // ChunkSize (will be updated)
        'W', 'A', 'V', 'E',  // Format
        'f', 'm', 't', ' ',  // Subchunk1ID
        16, 0, 0, 0,         // Subchunk1Size (16 for PCM)
        1, 0,                // AudioFormat (1 for PCM)
        1, 0,                // NumChannels (1 for mono)
    };

    // Sample rate (little endian)
    header[24] = m_sample_rate & 0xFF;
    header[25] = (m_sample_rate >> 8) & 0xFF;
    header[26] = (m_sample_rate >> 16) & 0xFF;
    header[27] = (m_sample_rate >> 24) & 0xFF;

    // Byte rate (SampleRate * NumChannels * BitsPerSample/8)
    uint32_t byte_rate = m_sample_rate * 1 * 2; // mono, 16-bit
    header[28] = byte_rate & 0xFF;
    header[29] = (byte_rate >> 8) & 0xFF;
    header[30] = (byte_rate >> 16) & 0xFF;
    header[31] = (byte_rate >> 24) & 0xFF;

    // Block align (NumChannels * BitsPerSample/8)
    header[32] = 2; // 1 channel * 16 bits / 8
    header[33] = 0;

    // Bits per sample
    header[34] = 16;
    header[35] = 0;

    // Data subchunk
    header[36] = 'd';
    header[37] = 'a';
    header[38] = 't';
    header[39] = 'a';
    // Data size will be updated later
    header[40] = 0;
    header[41] = 0;
    header[42] = 0;
    header[43] = 0;

    std::fwrite(header, 1, 44, m_file.get());

    if (g_debug_config.log_wav_export) {
        long pos_after = std::ftell(m_file.get());
        std::stringstream ss;
        ss << "WAV header written, file position now: " << pos_after;
        DEBUG_LOG_INFO("WAV", ss.str());
    }
}

void audio_file_writer::update_wav_header() {
    if (!m_file) return;

    uint32_t data_size = m_frames_written * sizeof(int16_t);
    uint32_t file_size = data_size + 36;

    if (g_debug_config.log_wav_export) {
        double duration_sec = static_cast<double>(m_frames_written) / m_sample_rate;
        long current_pos = std::ftell(m_file.get());
        std::stringstream ss;
        ss << "Updating WAV header: frames=" << m_frames_written
           << ", duration=" << duration_sec << "s"
           << ", data_size=" << data_size << " bytes"
           << ", file_size=" << file_size << " bytes"
           << ", current_file_pos=" << current_pos;
        DEBUG_LOG_INFO("WAV", ss.str());
    }

    // Update file size (at offset 4)
    int seek_result1 = std::fseek(m_file.get(), 4, SEEK_SET);
    if (g_debug_config.log_wav_export) {
        long pos = std::ftell(m_file.get());
        std::stringstream ss;
        ss << "Seeking to offset 4 (file_size), seek_result=" << seek_result1
           << ", position=" << pos;
        DEBUG_LOG_INFO("WAV", ss.str());
    }

    size_t written1 = std::fwrite(&file_size, 4, 1, m_file.get());
    if (g_debug_config.log_wav_export) {
        std::stringstream ss;
        ss << "Wrote file_size (" << file_size << "), bytes_written=" << (written1 * 4);
        DEBUG_LOG_INFO("WAV", ss.str());
    }

    // Update data size (at offset 40)
    int seek_result2 = std::fseek(m_file.get(), 40, SEEK_SET);
    if (g_debug_config.log_wav_export) {
        long pos = std::ftell(m_file.get());
        std::stringstream ss;
        ss << "Seeking to offset 40 (data_size), seek_result=" << seek_result2
           << ", position=" << pos;
        DEBUG_LOG_INFO("WAV", ss.str());
    }

    size_t written2 = std::fwrite(&data_size, 4, 1, m_file.get());
    if (g_debug_config.log_wav_export) {
        std::stringstream ss;
        ss << "Wrote data_size (" << data_size << "), bytes_written=" << (written2 * 4);
        DEBUG_LOG_INFO("WAV", ss.str());
    }

    // Flush to ensure data is written
    std::fflush(m_file.get());

    if (g_debug_config.log_wav_export) {
        std::stringstream ss;
        ss << "WAV header update complete, file flushed";
        DEBUG_LOG_INFO("WAV", ss.str());
    }
}

// Audio stream factory
std::unique_ptr<audio_stream> audio_stream_factory::create_stream(backend_type backend, const audio_stream::config& config) {
    if (backend == backend_type::AUTO) {
        // Auto-detect best backend
#ifdef HAVE_ALSA
        backend = backend_type::ALSA;
#elif defined(HAVE_DIRECTSOUND)
        backend = backend_type::DIRECTSOUND;
#else
        backend = backend_type::FILE_OUTPUT;
#endif
    }

    auto stream = std::make_unique<audio_stream>(config);

    // Backend-specific configuration
    if (backend == backend_type::FILE_OUTPUT) {
        stream->force_file_output_mode();
        // Set default output filename for testing
        stream->set_output_filename("/tmp/mame_synth_test_output.wav");
    }

    return stream;
}

std::vector<audio_stream_factory::backend_type> audio_stream_factory::get_available_backends() {
    std::vector<backend_type> backends;

#ifdef HAVE_ALSA
    backends.push_back(backend_type::ALSA);
#endif
#ifdef HAVE_DIRECTSOUND
    backends.push_back(backend_type::DIRECTSOUND);
#endif
    backends.push_back(backend_type::FILE_OUTPUT); // Always available

    return backends;
}

const char* audio_stream_factory::backend_name(backend_type backend) {
    switch (backend) {
        case backend_type::AUTO: return "Auto";
        case backend_type::ALSA: return "ALSA";
        case backend_type::DIRECTSOUND: return "DirectSound";
        case backend_type::FILE_OUTPUT: return "File Output";
        default: return "Unknown";
    }
}