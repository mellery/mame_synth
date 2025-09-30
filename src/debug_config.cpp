#include "debug_config.h"
#include <mutex>
#include <ctime>

// Global debug configuration instance
debug_config g_debug_config;

// Static member initialization
debug_config debug_logger::s_config;
std::ofstream debug_logger::s_log_file;
bool debug_logger::s_file_logging_enabled = false;

std::string debug_logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
    return ss.str();
}

std::string debug_logger::level_to_string(level lvl) {
    switch (lvl) {
        case level::INFO: return "INFO";
        case level::DEBUG: return "DEBUG";
        case level::WARNING: return "WARN";
        case level::ERROR: return "ERROR";
        case level::TIMING: return "TIME";
        case level::MIDI: return "MIDI";
        case level::AUDIO: return "AUDIO";
        case level::REGISTER: return "REG";
        default: return "UNKNOWN";
    }
}

void debug_logger::set_config(const debug_config& config) {
    s_config = config;
    g_debug_config = config;
}

const debug_config& debug_logger::get_config() {
    return s_config;
}

void debug_logger::enable_file_logging(const std::string& filename) {
    static std::mutex log_mutex;
    std::lock_guard<std::mutex> lock(log_mutex);

    if (s_log_file.is_open()) {
        s_log_file.close();
    }

    std::string log_filename = filename.empty() ?
        s_config.debug_log_prefix + "mame_synth.log" : filename;

    s_log_file.open(log_filename, std::ios::out | std::ios::app);
    s_file_logging_enabled = s_log_file.is_open();

    if (s_file_logging_enabled) {
        s_log_file << "\n=== Debug session started at " << get_timestamp() << " ===\n";
        s_log_file.flush();
    }
}

void debug_logger::disable_file_logging() {
    static std::mutex log_mutex;
    std::lock_guard<std::mutex> lock(log_mutex);

    if (s_log_file.is_open()) {
        s_log_file << "=== Debug session ended at " << get_timestamp() << " ===\n\n";
        s_log_file.close();
    }
    s_file_logging_enabled = false;
}

void debug_logger::log(level lvl, const std::string& category, const std::string& message) {
    static std::mutex log_mutex;
    std::lock_guard<std::mutex> lock(log_mutex);

    std::stringstream ss;
    ss << "[" << get_timestamp() << "] "
       << "[" << level_to_string(lvl) << "] "
       << "[" << category << "] "
       << message;

    std::string log_line = ss.str();

    // Always output to console
    std::cout << log_line << std::endl;

    // Optionally log to file
    if (s_file_logging_enabled && s_log_file.is_open()) {
        s_log_file << log_line << std::endl;
        s_log_file.flush();
    }
}

void debug_logger::log_midi_event(const std::string& message) {
    log(level::MIDI, "MIDI", message);
}

void debug_logger::log_register_write(uint32_t offset, uint8_t value, const std::string& description) {
    std::stringstream ss;
    ss << "Write 0x" << std::hex << std::setw(4) << std::setfill('0') << offset
       << " = 0x" << std::setw(2) << static_cast<int>(value);
    if (!description.empty()) {
        ss << " (" << description << ")";
    }
    log(level::REGISTER, "REGISTER", ss.str());
}

void debug_logger::log_audio_buffer(const std::string& message) {
    log(level::AUDIO, "AUDIO", message);
}

void debug_logger::log_timing(const std::string& message) {
    log(level::TIMING, "TIMING", message);
}

void debug_logger::log_performance(const std::string& operation, double duration_ms) {
    std::stringstream ss;
    ss << operation << " took " << std::fixed << std::setprecision(3)
       << duration_ms << " ms";
    log(level::TIMING, "PERF", ss.str());
}