#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <chrono>
#include <cstdint>
#include <cmath>

/**
 * Real-time Parameter Control System for NES Synthesizer
 *
 * Provides comprehensive real-time control over all NES synthesizer parameters:
 * - Channel volumes, panning, and effects
 * - Filter parameters and mixing settings
 * - Tempo, pitch, and timing adjustments
 * - Performance and quality settings
 * - MIDI controller mapping
 * - Parameter automation and modulation
 */

// Forward declarations - these classes are in global scope
class nes_playback_engine;
class nes_audio_mixer;
class nes_sequencer;

namespace nes_realtime {

/**
 * Parameter Types and Identifiers
 */
enum class parameter_type {
    // Global parameters
    MASTER_VOLUME,
    MASTER_TEMPO,
    MASTER_PITCH,

    // Channel parameters (per-channel)
    CHANNEL_VOLUME,
    CHANNEL_PAN,
    CHANNEL_MUTE,
    CHANNEL_SOLO,

    // NES-specific channel parameters
    PULSE_DUTY_CYCLE,
    PULSE_SWEEP_ENABLE,
    PULSE_SWEEP_RATE,
    TRIANGLE_LINEAR_COUNTER,
    NOISE_MODE,
    NOISE_PERIOD,
    DMC_RATE,
    DMC_LOOP,

    // Audio processing parameters
    HIGHPASS_CUTOFF,
    LOWPASS_CUTOFF,
    NONLINEAR_MIXING,
    REVERB_AMOUNT,
    STEREO_SEPARATION,

    // Performance parameters
    POLYPHONY_LIMIT,
    BUFFER_SIZE,
    LOOKAHEAD_TIME,

    // Automation parameters
    LFO_RATE,
    LFO_DEPTH,
    ENVELOPE_ATTACK,
    ENVELOPE_DECAY,
    ENVELOPE_SUSTAIN,
    ENVELOPE_RELEASE
};

/**
 * Parameter Value with Metadata
 */
struct parameter_value {
    float value = 0.0f;                    // Current parameter value
    float min_value = 0.0f;                // Minimum allowed value
    float max_value = 1.0f;                // Maximum allowed value
    float default_value = 0.0f;            // Default value
    std::string units = "";                // Units (Hz, dB, %, etc.)
    std::string description = "";          // Human-readable description
    bool is_discrete = false;              // Whether value should be integer
    std::vector<std::string> enum_values;  // For discrete parameters

    // Constructor
    parameter_value(float val = 0.0f, float min_val = 0.0f, float max_val = 1.0f,
                   const std::string& desc = "", const std::string& unit = "")
        : value(val), min_value(min_val), max_value(max_val), default_value(val)
        , units(unit), description(desc) {}

    // Validation and clamping
    void clamp() {
        if (value < min_value) value = min_value;
        if (value > max_value) value = max_value;
        if (is_discrete) value = std::round(value);
    }

    // Normalized value (0.0 to 1.0)
    float normalized() const {
        if (max_value <= min_value) return 0.0f;
        return (value - min_value) / (max_value - min_value);
    }

    void set_normalized(float norm_value) {
        value = min_value + norm_value * (max_value - min_value);
        clamp();
    }
};

/**
 * Parameter Change Event
 */
struct parameter_change {
    parameter_type type;
    uint8_t channel = 0;                   // Channel number (0-4 for NES)
    parameter_value old_value;
    parameter_value new_value;
    std::chrono::steady_clock::time_point timestamp;
    std::string source = "";               // Source of change (CLI, MIDI, automation, etc.)

    parameter_change(parameter_type t, const parameter_value& old_val, const parameter_value& new_val,
                    uint8_t ch = 0, const std::string& src = "")
        : type(t), channel(ch), old_value(old_val), new_value(new_val)
        , timestamp(std::chrono::steady_clock::now()), source(src) {}
};

/**
 * Parameter Change Listener Interface
 */
class parameter_listener {
public:
    virtual ~parameter_listener() = default;
    virtual void on_parameter_changed(const parameter_change& change) = 0;
    virtual void on_parameter_group_changed(const std::vector<parameter_change>& changes) {}
};

/**
 * MIDI Controller Mapping
 */
struct midi_mapping {
    uint8_t controller_number = 0;         // MIDI CC number (0-127)
    parameter_type parameter = parameter_type::MASTER_VOLUME;
    uint8_t channel = 0;                   // Target channel for channel parameters
    float scale = 1.0f;                    // Scaling factor
    float offset = 0.0f;                   // Offset value
    bool invert = false;                   // Invert the control

    midi_mapping() = default;
    midi_mapping(uint8_t cc, parameter_type param, uint8_t ch = 0)
        : controller_number(cc), parameter(param), channel(ch) {}
};

/**
 * Parameter Automation Curve
 */
class parameter_automation {
public:
    enum class curve_type {
        LINEAR,
        EXPONENTIAL,
        LOGARITHMIC,
        SINE_WAVE,
        SQUARE_WAVE,
        SAWTOOTH,
        TRIANGLE,
        CUSTOM_LUT  // Look-up table
    };

    struct automation_point {
        std::chrono::milliseconds time_offset;
        float value;
        curve_type interpolation = curve_type::LINEAR;

        automation_point(std::chrono::milliseconds time, float val, curve_type interp = curve_type::LINEAR)
            : time_offset(time), value(val), interpolation(interp) {}
    };

private:
    parameter_type m_parameter;
    uint8_t m_channel = 0;
    std::vector<automation_point> m_points;
    bool m_loop = false;
    std::chrono::steady_clock::time_point m_start_time;
    std::atomic<bool> m_active{false};

public:
    parameter_automation(parameter_type param, uint8_t channel = 0)
        : m_parameter(param), m_channel(channel) {}

    void add_point(std::chrono::milliseconds time, float value, curve_type interp = curve_type::LINEAR);
    void set_loop(bool loop) { m_loop = loop; }
    void start();
    void stop() { m_active = false; }
    bool is_active() const { return m_active; }

    float get_value_at_time(std::chrono::steady_clock::time_point current_time) const;
    parameter_type get_parameter() const { return m_parameter; }
    uint8_t get_channel() const { return m_channel; }
};

/**
 * Real-time Parameter Controller
 */
class realtime_parameter_controller {
public:
    // Control modes
    enum class control_mode {
        IMMEDIATE,      // Changes applied immediately
        QUANTIZED,      // Changes applied at musical boundaries
        SMOOTH,         // Changes applied with smoothing
        CROSSFADE       // Changes with crossfading
    };

    // Constructor
    explicit realtime_parameter_controller(control_mode mode = control_mode::SMOOTH);
    ~realtime_parameter_controller();

    // Engine integration
    void set_playback_engine(nes_playback_engine* engine);
    void set_audio_mixer(nes_audio_mixer* mixer);
    void set_sequencer(nes_sequencer* sequencer);

    // Parameter management
    bool register_parameter(parameter_type type, uint8_t channel, const parameter_value& default_value);
    bool set_parameter(parameter_type type, uint8_t channel, float value, const std::string& source = "");
    bool set_parameter_normalized(parameter_type type, uint8_t channel, float normalized_value, const std::string& source = "");
    parameter_value get_parameter(parameter_type type, uint8_t channel) const;
    std::vector<parameter_type> get_available_parameters() const;

    // Batch parameter changes
    void begin_parameter_group();
    void end_parameter_group();

    // Parameter presets
    void save_preset(const std::string& name);
    bool load_preset(const std::string& name);
    std::vector<std::string> get_preset_names() const;

    // MIDI control mapping
    void add_midi_mapping(const midi_mapping& mapping);
    void remove_midi_mapping(uint8_t controller_number);
    void process_midi_control_change(uint8_t controller, uint8_t value);
    std::vector<midi_mapping> get_midi_mappings() const;

    // Parameter automation
    void add_automation(std::unique_ptr<parameter_automation> automation);
    void remove_automation(parameter_type type, uint8_t channel);
    void clear_all_automations();
    void set_automation_enabled(bool enabled) { m_automation_enabled = enabled; }

    // Listeners
    void add_listener(std::shared_ptr<parameter_listener> listener);
    void remove_listener(std::shared_ptr<parameter_listener> listener);

    // Control settings
    void set_control_mode(control_mode mode) { m_control_mode = mode; }
    void set_smoothing_time(std::chrono::milliseconds time) { m_smoothing_time = time; }
    void set_quantization_enabled(bool enabled) { m_quantization_enabled = enabled; }

    // Real-time processing
    void start_processing();
    void stop_processing();
    void process_frame();  // Called once per audio frame

    // Statistics and monitoring
    struct control_stats {
        uint64_t parameters_changed = 0;
        uint64_t midi_messages_processed = 0;
        uint64_t automation_updates = 0;
        double average_processing_time_us = 0.0;
        uint32_t active_automations = 0;
        uint32_t registered_parameters = 0;
    };
    control_stats get_stats() const;
    void reset_stats();

private:
    // Parameter storage
    using parameter_key = std::pair<parameter_type, uint8_t>;
    std::map<parameter_key, parameter_value> m_parameters;
    mutable std::mutex m_parameters_mutex;

    // Smoothing and interpolation
    struct smoothed_parameter {
        float current_value = 0.0f;
        float target_value = 0.0f;
        float rate = 0.0f;  // Change rate per frame
        bool active = false;
    };
    std::map<parameter_key, smoothed_parameter> m_smoothed_parameters;

    // Engine references
    nes_playback_engine* m_playback_engine = nullptr;
    nes_audio_mixer* m_audio_mixer = nullptr;
    nes_sequencer* m_sequencer = nullptr;

    // Control settings
    control_mode m_control_mode = control_mode::SMOOTH;
    std::chrono::milliseconds m_smoothing_time{50};
    bool m_quantization_enabled = false;

    // MIDI mappings
    std::map<uint8_t, midi_mapping> m_midi_mappings;
    mutable std::mutex m_midi_mutex;

    // Automation system
    std::vector<std::unique_ptr<parameter_automation>> m_automations;
    std::atomic<bool> m_automation_enabled{true};
    mutable std::mutex m_automation_mutex;

    // Listeners
    std::vector<std::weak_ptr<parameter_listener>> m_listeners;
    mutable std::mutex m_listeners_mutex;

    // Parameter grouping
    std::vector<parameter_change> m_parameter_group;
    bool m_in_parameter_group = false;

    // Processing thread
    std::thread m_processing_thread;
    std::atomic<bool> m_processing_active{false};

    // Change queue for thread-safe communication
    std::queue<parameter_change> m_change_queue;
    std::mutex m_queue_mutex;

    // Statistics
    mutable control_stats m_stats;
    mutable std::mutex m_stats_mutex;
    std::chrono::steady_clock::time_point m_last_frame_time;

    // Presets
    std::map<std::string, std::map<parameter_key, parameter_value>> m_presets;

    // Internal methods
    void processing_thread_func();
    void apply_parameter_change(const parameter_change& change);
    void notify_listeners(const parameter_change& change);
    void notify_listeners_group(const std::vector<parameter_change>& changes);
    void update_smoothed_parameters();
    void update_automations();
    float interpolate_value(float current, float target, float rate) const;
    std::string parameter_key_to_string(parameter_type type, uint8_t channel) const;
    void apply_to_engine(parameter_type type, uint8_t channel, float value);
    void register_default_parameters();
};

/**
 * Real-time Control Interface - High-level convenience functions
 */
class nes_realtime_control_interface {
public:
    explicit nes_realtime_control_interface(realtime_parameter_controller& controller);

    // Convenience methods for common operations
    void set_master_volume(float volume);
    void set_master_tempo(float tempo_scale);
    void set_channel_volume(uint8_t channel, float volume);
    void set_channel_pan(uint8_t channel, float pan);
    void mute_channel(uint8_t channel, bool mute);
    void solo_channel(uint8_t channel, bool solo);

    // NES-specific controls
    void set_pulse_duty_cycle(uint8_t pulse_channel, uint8_t duty);  // 0-3
    void set_triangle_linear_counter(uint8_t value);                 // 0-127
    void set_noise_mode(bool short_mode);
    void set_noise_period(uint8_t period);                          // 0-15
    void enable_pulse_sweep(uint8_t pulse_channel, bool enable, uint8_t rate = 0);

    // Audio processing controls
    void set_highpass_cutoff(float frequency);
    void set_lowpass_cutoff(float frequency);
    void set_nonlinear_mixing(bool enable);
    void set_reverb_amount(float amount);
    void set_stereo_separation(float amount);

    // Quick parameter snapshots
    void save_channel_snapshot(uint8_t channel, const std::string& name);
    void restore_channel_snapshot(uint8_t channel, const std::string& name);
    void save_global_snapshot(const std::string& name);
    void restore_global_snapshot(const std::string& name);

    // Parameter modulation helpers
    void start_volume_fade(uint8_t channel, float target_volume, std::chrono::milliseconds duration);
    void start_tempo_ramp(float target_tempo, std::chrono::milliseconds duration);
    void start_filter_sweep(float start_freq, float end_freq, std::chrono::milliseconds duration);

    // Real-time effects
    void trigger_channel_tremolo(uint8_t channel, float depth, float rate, std::chrono::milliseconds duration);
    void trigger_global_vibrato(float depth, float rate, std::chrono::milliseconds duration);

private:
    realtime_parameter_controller& m_controller;
    std::map<std::string, std::map<std::string, float>> m_snapshots;
};

/**
 * MIDI Real-time Control Handler
 */
class midi_realtime_handler {
public:
    explicit midi_realtime_handler(realtime_parameter_controller& controller);

    // MIDI message processing
    void process_control_change(uint8_t channel, uint8_t controller, uint8_t value);
    void process_pitch_bend(uint8_t channel, uint16_t bend_value);
    void process_aftertouch(uint8_t channel, uint8_t pressure);
    void process_program_change(uint8_t channel, uint8_t program);

    // Standard MIDI controller mappings
    void setup_default_mappings();
    void setup_nes_specific_mappings();

    // Learn mode for easy mapping
    void enter_learn_mode(parameter_type parameter, uint8_t channel = 0);
    void exit_learn_mode();
    bool is_in_learn_mode() const { return m_learn_mode; }

private:
    realtime_parameter_controller& m_controller;
    bool m_learn_mode = false;
    parameter_type m_learn_parameter = parameter_type::MASTER_VOLUME;
    uint8_t m_learn_channel = 0;
};

} // namespace nes_realtime