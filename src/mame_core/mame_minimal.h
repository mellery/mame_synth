#pragma once

/**
 * Minimal MAME Core Compatibility Layer
 *
 * This provides minimal implementations of MAME core types and functions
 * needed for audio device integration without requiring the full MAME build system.
 *
 * This file allows gradual integration with real MAME devices while maintaining
 * build compatibility and testing capability.
 */

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>

// Basic MAME types
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using offs_t = u32;
using endianness_t = int;

// Basic MAME constants
static constexpr endianness_t ENDIANNESS_LITTLE = 0;
static constexpr endianness_t ENDIANNESS_BIG = 1;

// Forward declarations of MAME classes we'll need
class device_t;
class machine_config;
class running_machine;
class device_sound_interface;
class sound_stream;

/**
 * Minimal MAME machine configuration
 * Provides basic machine context needed for audio devices
 */
class minimal_machine_config {
public:
    minimal_machine_config();
    ~minimal_machine_config();

    // Basic machine properties
    u32 get_sample_rate() const { return m_sample_rate; }
    void set_sample_rate(u32 rate) { m_sample_rate = rate; }

    // Device registration
    void register_device(device_t* device);
    void unregister_device(device_t* device);

    // Get registered device count
    size_t device_count() const { return m_devices.size(); }

private:
    u32 m_sample_rate = 44100;
    std::vector<device_t*> m_devices;
};

/**
 * Minimal MAME device base class
 * Provides essential device functionality for audio devices
 */
class minimal_device_t {
public:
    minimal_device_t(minimal_machine_config* config, const char* tag, u32 clock);
    virtual ~minimal_device_t();

    // Device lifecycle
    virtual void device_start() {}
    virtual void device_reset() {}
    virtual void device_stop() {}

    // Device properties
    const std::string& tag() const { return m_tag; }
    u32 clock() const { return m_clock; }
    minimal_machine_config* machine_config() const { return m_config; }

    // Device state
    bool started() const { return m_started; }
    void set_started(bool started) { m_started = started; }

protected:
    minimal_machine_config* m_config;
    std::string m_tag;
    u32 m_clock;
    bool m_started = false;
};

/**
 * Minimal sound interface
 * Provides basic sound streaming capability
 */
class minimal_device_sound_interface {
public:
    minimal_device_sound_interface(minimal_device_t* device);
    virtual ~minimal_device_sound_interface();

    // Sound generation
    virtual void sound_stream_update(s16* buffer, size_t samples) = 0;
    virtual u32 sample_rate() const { return 44100; }

    // Stream management
    void allocate_stream(int inputs, int outputs);
    bool has_stream() const { return m_stream_allocated; }

protected:
    minimal_device_t* m_device;
    bool m_stream_allocated = false;
};

/**
 * Minimal callback/delegate system
 * Provides basic callback functionality for device interaction
 */
template<typename T>
class minimal_callback {
public:
    minimal_callback() = default;

    template<typename F>
    void bind(F&& func) {
        m_callback = std::forward<F>(func);
    }

    auto bind() {
        return [this](auto&&... args) -> decltype(auto) {
            if (m_callback) {
                return m_callback(std::forward<decltype(args)>(args)...);
            }
            return T{};
        };
    }

    bool is_bound() const { return static_cast<bool>(m_callback); }

    template<typename... Args>
    T operator()(Args&&... args) const {
        if (m_callback) {
            return m_callback(std::forward<Args>(args)...);
        }
        return T{};
    }

private:
    std::function<T()> m_callback;
};

// Common MAME callback types
using write8_delegate = minimal_callback<void>;
using read8_delegate = minimal_callback<u8>;

/**
 * Global minimal machine instance
 * Provides access to the current machine context
 */
class minimal_machine_manager {
public:
    static minimal_machine_manager& instance();

    void set_machine_config(minimal_machine_config* config);
    minimal_machine_config* machine_config() const { return m_config; }

    bool is_initialized() const { return m_config != nullptr; }

private:
    minimal_machine_config* m_config = nullptr;
};

// Convenience macros for MAME compatibility
#define ATTR_COLD
#define DECLARE_DEVICE_TYPE(name, type) extern const device_type name;
#define DEFINE_DEVICE_TYPE(name, type, shortname, fullname) const device_type name = nullptr;

// Utility functions
inline std::string string_format(const char* format, ...) {
    // Simple format implementation for basic logging
    return std::string(format);
}