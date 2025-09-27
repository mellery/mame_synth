#include "nes_realtime_control.h"
#include "nes_playback_engine.h"
#include "nes_audio_mixer.h"
#include "nes_sequencer.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace nes_realtime {

// Parameter Automation Implementation
void parameter_automation::add_point(std::chrono::milliseconds time, float value, curve_type interp) {
    m_points.emplace_back(time, value, interp);
    // Keep points sorted by time
    std::sort(m_points.begin(), m_points.end(),
              [](const automation_point& a, const automation_point& b) {
                  return a.time_offset < b.time_offset;
              });
}

void parameter_automation::start() {
    m_start_time = std::chrono::steady_clock::now();
    m_active = true;
}

float parameter_automation::get_value_at_time(std::chrono::steady_clock::time_point current_time) const {
    if (!m_active || m_points.empty()) {
        return 0.0f;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - m_start_time);

    // Handle looping
    if (m_loop && !m_points.empty()) {
        auto total_duration = m_points.back().time_offset;
        if (total_duration.count() > 0) {
            elapsed = std::chrono::milliseconds(elapsed.count() % total_duration.count());
        }
    }

    // Find the appropriate point(s) for interpolation
    if (elapsed <= m_points.front().time_offset) {
        return m_points.front().value;
    }
    if (elapsed >= m_points.back().time_offset) {
        return m_loop ? m_points.front().value : m_points.back().value;
    }

    // Find interpolation range
    for (size_t i = 0; i < m_points.size() - 1; ++i) {
        if (elapsed >= m_points[i].time_offset && elapsed <= m_points[i + 1].time_offset) {
            const auto& p1 = m_points[i];
            const auto& p2 = m_points[i + 1];

            auto duration = p2.time_offset - p1.time_offset;
            if (duration.count() == 0) return p1.value;

            float t = static_cast<float>((elapsed - p1.time_offset).count()) / duration.count();

            // Apply interpolation curve
            switch (p1.interpolation) {
                case curve_type::LINEAR:
                    return p1.value + t * (p2.value - p1.value);

                case curve_type::EXPONENTIAL:
                    return p1.value + (p2.value - p1.value) * (t * t);

                case curve_type::LOGARITHMIC:
                    return p1.value + (p2.value - p1.value) * std::sqrt(t);

                case curve_type::SINE_WAVE:
                    return p1.value + (p2.value - p1.value) * (0.5f + 0.5f * std::sin(t * 2.0f * M_PI - M_PI_2));

                case curve_type::SQUARE_WAVE:
                    return (t < 0.5f) ? p1.value : p2.value;

                case curve_type::SAWTOOTH:
                    return p1.value + t * (p2.value - p1.value);

                case curve_type::TRIANGLE:
                    if (t < 0.5f) {
                        return p1.value + 2.0f * t * (p2.value - p1.value);
                    } else {
                        return p2.value - 2.0f * (t - 0.5f) * (p2.value - p1.value);
                    }

                default:
                    return p1.value + t * (p2.value - p1.value);
            }
        }
    }

    return m_points.back().value;
}

// Realtime Parameter Controller Implementation
realtime_parameter_controller::realtime_parameter_controller(control_mode mode)
    : m_control_mode(mode)
    , m_last_frame_time(std::chrono::steady_clock::now()) {
    register_default_parameters();
}

realtime_parameter_controller::~realtime_parameter_controller() {
    stop_processing();
}

void realtime_parameter_controller::set_playback_engine(::nes_playback_engine* engine) {
    m_playback_engine = engine;
}

void realtime_parameter_controller::set_audio_mixer(::nes_audio_mixer* mixer) {
    m_audio_mixer = mixer;
}

void realtime_parameter_controller::set_sequencer(::nes_sequencer* sequencer) {
    m_sequencer = sequencer;
}

bool realtime_parameter_controller::register_parameter(parameter_type type, uint8_t channel, const parameter_value& default_value) {
    std::lock_guard<std::mutex> lock(m_parameters_mutex);
    parameter_key key = {type, channel};
    m_parameters[key] = default_value;

    // Initialize smoothed parameter
    m_smoothed_parameters[key] = {default_value.value, default_value.value, 0.0f, false};

    return true;
}

bool realtime_parameter_controller::set_parameter(parameter_type type, uint8_t channel, float value, const std::string& source) {
    parameter_key key = {type, channel};

    std::lock_guard<std::mutex> lock(m_parameters_mutex);
    auto it = m_parameters.find(key);
    if (it == m_parameters.end()) {
        return false;  // Parameter not registered
    }

    parameter_value old_value = it->second;
    it->second.value = value;
    it->second.clamp();

    // Create change event
    parameter_change change(type, old_value, it->second, channel, source);

    if (m_in_parameter_group) {
        m_parameter_group.push_back(change);
    } else {
        // Queue for processing
        std::lock_guard<std::mutex> queue_lock(m_queue_mutex);
        m_change_queue.push(change);
    }

    return true;
}

bool realtime_parameter_controller::set_parameter_normalized(parameter_type type, uint8_t channel, float normalized_value, const std::string& source) {
    parameter_key key = {type, channel};

    std::lock_guard<std::mutex> lock(m_parameters_mutex);
    auto it = m_parameters.find(key);
    if (it == m_parameters.end()) {
        return false;
    }

    parameter_value old_value = it->second;
    it->second.set_normalized(normalized_value);

    parameter_change change(type, old_value, it->second, channel, source);

    if (m_in_parameter_group) {
        m_parameter_group.push_back(change);
    } else {
        std::lock_guard<std::mutex> queue_lock(m_queue_mutex);
        m_change_queue.push(change);
    }

    return true;
}

parameter_value realtime_parameter_controller::get_parameter(parameter_type type, uint8_t channel) const {
    std::lock_guard<std::mutex> lock(m_parameters_mutex);
    parameter_key key = {type, channel};
    auto it = m_parameters.find(key);
    return (it != m_parameters.end()) ? it->second : parameter_value{};
}

std::vector<parameter_type> realtime_parameter_controller::get_available_parameters() const {
    std::lock_guard<std::mutex> lock(m_parameters_mutex);
    std::vector<parameter_type> types;
    for (const auto& pair : m_parameters) {
        types.push_back(pair.first.first);
    }

    // Remove duplicates
    std::sort(types.begin(), types.end());
    types.erase(std::unique(types.begin(), types.end()), types.end());

    return types;
}

void realtime_parameter_controller::begin_parameter_group() {
    m_in_parameter_group = true;
    m_parameter_group.clear();
}

void realtime_parameter_controller::end_parameter_group() {
    if (m_in_parameter_group && !m_parameter_group.empty()) {
        notify_listeners_group(m_parameter_group);
        m_parameter_group.clear();
    }
    m_in_parameter_group = false;
}

void realtime_parameter_controller::save_preset(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_parameters_mutex);
    m_presets[name] = m_parameters;
}

bool realtime_parameter_controller::load_preset(const std::string& name) {
    auto it = m_presets.find(name);
    if (it == m_presets.end()) {
        return false;
    }

    begin_parameter_group();
    for (const auto& param : it->second) {
        set_parameter(param.first.first, param.first.second, param.second.value, "preset:" + name);
    }
    end_parameter_group();

    return true;
}

std::vector<std::string> realtime_parameter_controller::get_preset_names() const {
    std::vector<std::string> names;
    for (const auto& preset : m_presets) {
        names.push_back(preset.first);
    }
    return names;
}

void realtime_parameter_controller::add_midi_mapping(const midi_mapping& mapping) {
    std::lock_guard<std::mutex> lock(m_midi_mutex);
    m_midi_mappings[mapping.controller_number] = mapping;
}

void realtime_parameter_controller::remove_midi_mapping(uint8_t controller_number) {
    std::lock_guard<std::mutex> lock(m_midi_mutex);
    m_midi_mappings.erase(controller_number);
}

void realtime_parameter_controller::process_midi_control_change(uint8_t controller, uint8_t value) {
    std::lock_guard<std::mutex> lock(m_midi_mutex);
    auto it = m_midi_mappings.find(controller);
    if (it == m_midi_mappings.end()) {
        return;
    }

    const auto& mapping = it->second;

    // Convert MIDI value (0-127) to normalized value (0.0-1.0)
    float normalized = static_cast<float>(value) / 127.0f;
    if (mapping.invert) {
        normalized = 1.0f - normalized;
    }

    // Apply scaling and offset
    normalized = normalized * mapping.scale + mapping.offset;
    normalized = std::clamp(normalized, 0.0f, 1.0f);

    set_parameter_normalized(mapping.parameter, mapping.channel, normalized, "MIDI CC" + std::to_string(controller));

    // Update statistics
    std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
    m_stats.midi_messages_processed++;
}

std::vector<midi_mapping> realtime_parameter_controller::get_midi_mappings() const {
    std::lock_guard<std::mutex> lock(m_midi_mutex);
    std::vector<midi_mapping> mappings;
    for (const auto& pair : m_midi_mappings) {
        mappings.push_back(pair.second);
    }
    return mappings;
}

void realtime_parameter_controller::add_automation(std::unique_ptr<parameter_automation> automation) {
    std::lock_guard<std::mutex> lock(m_automation_mutex);
    m_automations.push_back(std::move(automation));
}

void realtime_parameter_controller::remove_automation(parameter_type type, uint8_t channel) {
    std::lock_guard<std::mutex> lock(m_automation_mutex);
    m_automations.erase(
        std::remove_if(m_automations.begin(), m_automations.end(),
                      [type, channel](const std::unique_ptr<parameter_automation>& automation) {
                          return automation->get_parameter() == type && automation->get_channel() == channel;
                      }),
        m_automations.end());
}

void realtime_parameter_controller::clear_all_automations() {
    std::lock_guard<std::mutex> lock(m_automation_mutex);
    m_automations.clear();
}

void realtime_parameter_controller::add_listener(std::shared_ptr<parameter_listener> listener) {
    std::lock_guard<std::mutex> lock(m_listeners_mutex);
    m_listeners.push_back(listener);
}

void realtime_parameter_controller::remove_listener(std::shared_ptr<parameter_listener> listener) {
    std::lock_guard<std::mutex> lock(m_listeners_mutex);
    m_listeners.erase(
        std::remove_if(m_listeners.begin(), m_listeners.end(),
                      [&listener](const std::weak_ptr<parameter_listener>& weak_ptr) {
                          return weak_ptr.lock() == listener;
                      }),
        m_listeners.end());
}

void realtime_parameter_controller::start_processing() {
    if (m_processing_active) {
        return;
    }

    m_processing_active = true;
    m_processing_thread = std::thread(&realtime_parameter_controller::processing_thread_func, this);
}

void realtime_parameter_controller::stop_processing() {
    if (!m_processing_active) {
        return;
    }

    m_processing_active = false;
    if (m_processing_thread.joinable()) {
        m_processing_thread.join();
    }
}

void realtime_parameter_controller::process_frame() {
    auto frame_start = std::chrono::steady_clock::now();

    // Process queued parameter changes
    std::queue<parameter_change> local_queue;
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        local_queue.swap(m_change_queue);
    }

    while (!local_queue.empty()) {
        apply_parameter_change(local_queue.front());
        local_queue.pop();
    }

    // Update smoothed parameters
    update_smoothed_parameters();

    // Update automations
    if (m_automation_enabled) {
        update_automations();
    }

    // Update performance statistics
    auto frame_end = std::chrono::steady_clock::now();
    auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(frame_end - frame_start);

    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_stats.average_processing_time_us =
        (m_stats.average_processing_time_us * 0.99) + (frame_duration.count() * 0.01);

    m_last_frame_time = frame_end;
}

realtime_parameter_controller::control_stats realtime_parameter_controller::get_stats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    auto stats = m_stats;

    // Update active automation count
    std::lock_guard<std::mutex> automation_lock(m_automation_mutex);
    stats.active_automations = static_cast<uint32_t>(
        std::count_if(m_automations.begin(), m_automations.end(),
                     [](const std::unique_ptr<parameter_automation>& automation) {
                         return automation->is_active();
                     }));

    // Update registered parameter count
    std::lock_guard<std::mutex> param_lock(m_parameters_mutex);
    stats.registered_parameters = static_cast<uint32_t>(m_parameters.size());

    return stats;
}

void realtime_parameter_controller::reset_stats() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_stats = control_stats{};
}

void realtime_parameter_controller::processing_thread_func() {
    while (m_processing_active) {
        process_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));  // ~1000 FPS processing
    }
}

void realtime_parameter_controller::apply_parameter_change(const parameter_change& change) {
    // Apply to smoothed parameters if smoothing is enabled
    if (m_control_mode == control_mode::SMOOTH) {
        parameter_key key = {change.type, change.channel};
        auto it = m_smoothed_parameters.find(key);
        if (it != m_smoothed_parameters.end()) {
            it->second.target_value = change.new_value.value;
            it->second.active = true;

            // Calculate smoothing rate
            float distance = std::abs(it->second.target_value - it->second.current_value);
            if (distance > 0.001f) {  // Avoid division by zero
                // Assuming 48kHz sample rate and desired smoothing time
                float frames_to_smooth = (m_smoothing_time.count() / 1000.0f) * 48000.0f;
                it->second.rate = distance / frames_to_smooth;
            }
        }
    } else {
        // Apply immediately
        apply_to_engine(change.type, change.channel, change.new_value.value);
    }

    notify_listeners(change);

    // Update statistics
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_stats.parameters_changed++;
}

void realtime_parameter_controller::notify_listeners(const parameter_change& change) {
    std::lock_guard<std::mutex> lock(m_listeners_mutex);
    for (auto it = m_listeners.begin(); it != m_listeners.end();) {
        if (auto listener = it->lock()) {
            listener->on_parameter_changed(change);
            ++it;
        } else {
            it = m_listeners.erase(it);  // Remove expired weak_ptr
        }
    }
}

void realtime_parameter_controller::notify_listeners_group(const std::vector<parameter_change>& changes) {
    std::lock_guard<std::mutex> lock(m_listeners_mutex);
    for (auto it = m_listeners.begin(); it != m_listeners.end();) {
        if (auto listener = it->lock()) {
            listener->on_parameter_group_changed(changes);
            ++it;
        } else {
            it = m_listeners.erase(it);
        }
    }
}

void realtime_parameter_controller::update_smoothed_parameters() {
    for (auto& pair : m_smoothed_parameters) {
        auto& smoothed = pair.second;
        if (!smoothed.active) continue;

        float distance = smoothed.target_value - smoothed.current_value;
        if (std::abs(distance) < 0.001f) {
            smoothed.current_value = smoothed.target_value;
            smoothed.active = false;
        } else {
            float direction = (distance > 0) ? 1.0f : -1.0f;
            smoothed.current_value += direction * std::min(smoothed.rate, std::abs(distance));
        }

        // Apply to engine
        apply_to_engine(pair.first.first, pair.first.second, smoothed.current_value);
    }
}

void realtime_parameter_controller::update_automations() {
    std::lock_guard<std::mutex> lock(m_automation_mutex);
    auto current_time = std::chrono::steady_clock::now();

    for (const auto& automation : m_automations) {
        if (automation->is_active()) {
            float value = automation->get_value_at_time(current_time);
            set_parameter(automation->get_parameter(), automation->get_channel(), value, "automation");

            // Update statistics
            std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
            m_stats.automation_updates++;
        }
    }
}

void realtime_parameter_controller::apply_to_engine(parameter_type type, uint8_t channel, float value) {
    switch (type) {
        case parameter_type::MASTER_VOLUME:
            if (m_playback_engine) {
                m_playback_engine->set_master_volume(value);
            }
            break;

        case parameter_type::MASTER_TEMPO:
            if (m_playback_engine) {
                m_playback_engine->set_tempo_scale(value);
            }
            break;

        case parameter_type::CHANNEL_VOLUME:
            if (m_playback_engine) {
                m_playback_engine->set_channel_volume(channel, value);
            }
            break;

        case parameter_type::CHANNEL_MUTE:
            if (m_playback_engine) {
                m_playback_engine->mute_channel(channel, value > 0.5f);
            }
            break;

        case parameter_type::PULSE_DUTY_CYCLE:
            if (m_playback_engine && channel < 2) {  // Only for pulse channels
                m_playback_engine->set_pulse_duty_cycle(channel, static_cast<uint8_t>(value));
            }
            break;

        case parameter_type::TRIANGLE_LINEAR_COUNTER:
            if (m_playback_engine) {
                m_playback_engine->set_triangle_linear_counter(static_cast<uint8_t>(value));
            }
            break;

        case parameter_type::NOISE_MODE:
            if (m_playback_engine) {
                m_playback_engine->set_noise_mode(value > 0.5f);
            }
            break;

        case parameter_type::HIGHPASS_CUTOFF:
        case parameter_type::LOWPASS_CUTOFF:
        case parameter_type::NONLINEAR_MIXING:
        case parameter_type::REVERB_AMOUNT:
        case parameter_type::STEREO_SEPARATION:
            // These would be applied to the audio mixer
            // Implementation depends on audio mixer interface
            break;

        default:
            // Handle other parameter types
            break;
    }
}

void realtime_parameter_controller::register_default_parameters() {
    // Global parameters
    register_parameter(parameter_type::MASTER_VOLUME, 0, parameter_value(1.0f, 0.0f, 2.0f, "Master volume", ""));
    register_parameter(parameter_type::MASTER_TEMPO, 0, parameter_value(1.0f, 0.1f, 4.0f, "Tempo scale", "x"));
    register_parameter(parameter_type::MASTER_PITCH, 0, parameter_value(0.0f, -12.0f, 12.0f, "Pitch shift", "semitones"));

    // Channel parameters for all 5 NES channels
    for (uint8_t ch = 0; ch < 5; ++ch) {
        register_parameter(parameter_type::CHANNEL_VOLUME, ch, parameter_value(1.0f, 0.0f, 2.0f, "Channel volume", ""));
        register_parameter(parameter_type::CHANNEL_PAN, ch, parameter_value(0.0f, -1.0f, 1.0f, "Channel pan", ""));
        register_parameter(parameter_type::CHANNEL_MUTE, ch, parameter_value(0.0f, 0.0f, 1.0f, "Channel mute", ""));
        register_parameter(parameter_type::CHANNEL_SOLO, ch, parameter_value(0.0f, 0.0f, 1.0f, "Channel solo", ""));
    }

    // NES-specific parameters
    register_parameter(parameter_type::PULSE_DUTY_CYCLE, 0, parameter_value(2.0f, 0.0f, 3.0f, "Pulse 1 duty cycle", ""));
    register_parameter(parameter_type::PULSE_DUTY_CYCLE, 1, parameter_value(2.0f, 0.0f, 3.0f, "Pulse 2 duty cycle", ""));
    register_parameter(parameter_type::TRIANGLE_LINEAR_COUNTER, 0, parameter_value(127.0f, 0.0f, 127.0f, "Triangle linear counter", ""));
    register_parameter(parameter_type::NOISE_MODE, 0, parameter_value(0.0f, 0.0f, 1.0f, "Noise mode", ""));
    register_parameter(parameter_type::NOISE_PERIOD, 0, parameter_value(8.0f, 0.0f, 15.0f, "Noise period", ""));

    // Audio processing parameters
    register_parameter(parameter_type::HIGHPASS_CUTOFF, 0, parameter_value(90.0f, 1.0f, 1000.0f, "Highpass cutoff", "Hz"));
    register_parameter(parameter_type::LOWPASS_CUTOFF, 0, parameter_value(14000.0f, 1000.0f, 24000.0f, "Lowpass cutoff", "Hz"));
    register_parameter(parameter_type::NONLINEAR_MIXING, 0, parameter_value(1.0f, 0.0f, 1.0f, "Nonlinear mixing", ""));
    register_parameter(parameter_type::REVERB_AMOUNT, 0, parameter_value(0.0f, 0.0f, 1.0f, "Reverb amount", ""));
    register_parameter(parameter_type::STEREO_SEPARATION, 0, parameter_value(0.0f, 0.0f, 1.0f, "Stereo separation", ""));
}

// Real-time Control Interface Implementation
nes_realtime_control_interface::nes_realtime_control_interface(realtime_parameter_controller& controller)
    : m_controller(controller) {}

void nes_realtime_control_interface::set_master_volume(float volume) {
    m_controller.set_parameter(parameter_type::MASTER_VOLUME, 0, volume, "interface");
}

void nes_realtime_control_interface::set_master_tempo(float tempo_scale) {
    m_controller.set_parameter(parameter_type::MASTER_TEMPO, 0, tempo_scale, "interface");
}

void nes_realtime_control_interface::set_channel_volume(uint8_t channel, float volume) {
    m_controller.set_parameter(parameter_type::CHANNEL_VOLUME, channel, volume, "interface");
}

void nes_realtime_control_interface::set_channel_pan(uint8_t channel, float pan) {
    m_controller.set_parameter(parameter_type::CHANNEL_PAN, channel, pan, "interface");
}

void nes_realtime_control_interface::mute_channel(uint8_t channel, bool mute) {
    m_controller.set_parameter(parameter_type::CHANNEL_MUTE, channel, mute ? 1.0f : 0.0f, "interface");
}

void nes_realtime_control_interface::solo_channel(uint8_t channel, bool solo) {
    m_controller.set_parameter(parameter_type::CHANNEL_SOLO, channel, solo ? 1.0f : 0.0f, "interface");
}

void nes_realtime_control_interface::set_pulse_duty_cycle(uint8_t pulse_channel, uint8_t duty) {
    if (pulse_channel < 2) {
        m_controller.set_parameter(parameter_type::PULSE_DUTY_CYCLE, pulse_channel, static_cast<float>(duty), "interface");
    }
}

void nes_realtime_control_interface::set_triangle_linear_counter(uint8_t value) {
    m_controller.set_parameter(parameter_type::TRIANGLE_LINEAR_COUNTER, 0, static_cast<float>(value), "interface");
}

void nes_realtime_control_interface::set_noise_mode(bool short_mode) {
    m_controller.set_parameter(parameter_type::NOISE_MODE, 0, short_mode ? 1.0f : 0.0f, "interface");
}

void nes_realtime_control_interface::set_noise_period(uint8_t period) {
    m_controller.set_parameter(parameter_type::NOISE_PERIOD, 0, static_cast<float>(period), "interface");
}

void nes_realtime_control_interface::enable_pulse_sweep(uint8_t pulse_channel, bool enable, uint8_t rate) {
    if (pulse_channel < 2) {
        m_controller.set_parameter(parameter_type::PULSE_SWEEP_ENABLE, pulse_channel, enable ? 1.0f : 0.0f, "interface");
        if (enable) {
            m_controller.set_parameter(parameter_type::PULSE_SWEEP_RATE, pulse_channel, static_cast<float>(rate), "interface");
        }
    }
}

void nes_realtime_control_interface::set_highpass_cutoff(float frequency) {
    m_controller.set_parameter(parameter_type::HIGHPASS_CUTOFF, 0, frequency, "interface");
}

void nes_realtime_control_interface::set_lowpass_cutoff(float frequency) {
    m_controller.set_parameter(parameter_type::LOWPASS_CUTOFF, 0, frequency, "interface");
}

void nes_realtime_control_interface::set_nonlinear_mixing(bool enable) {
    m_controller.set_parameter(parameter_type::NONLINEAR_MIXING, 0, enable ? 1.0f : 0.0f, "interface");
}

void nes_realtime_control_interface::set_reverb_amount(float amount) {
    m_controller.set_parameter(parameter_type::REVERB_AMOUNT, 0, amount, "interface");
}

void nes_realtime_control_interface::set_stereo_separation(float amount) {
    m_controller.set_parameter(parameter_type::STEREO_SEPARATION, 0, amount, "interface");
}

void nes_realtime_control_interface::save_channel_snapshot(uint8_t channel, const std::string& name) {
    std::map<std::string, float> snapshot;
    snapshot["volume"] = m_controller.get_parameter(parameter_type::CHANNEL_VOLUME, channel).value;
    snapshot["pan"] = m_controller.get_parameter(parameter_type::CHANNEL_PAN, channel).value;
    snapshot["mute"] = m_controller.get_parameter(parameter_type::CHANNEL_MUTE, channel).value;

    m_snapshots["channel_" + std::to_string(channel) + "_" + name] = snapshot;
}

void nes_realtime_control_interface::restore_channel_snapshot(uint8_t channel, const std::string& name) {
    auto key = "channel_" + std::to_string(channel) + "_" + name;
    auto it = m_snapshots.find(key);
    if (it != m_snapshots.end()) {
        const auto& snapshot = it->second;

        m_controller.begin_parameter_group();
        for (const auto& param : snapshot) {
            if (param.first == "volume") {
                m_controller.set_parameter(parameter_type::CHANNEL_VOLUME, channel, param.second, "snapshot");
            } else if (param.first == "pan") {
                m_controller.set_parameter(parameter_type::CHANNEL_PAN, channel, param.second, "snapshot");
            } else if (param.first == "mute") {
                m_controller.set_parameter(parameter_type::CHANNEL_MUTE, channel, param.second, "snapshot");
            }
        }
        m_controller.end_parameter_group();
    }
}

void nes_realtime_control_interface::save_global_snapshot(const std::string& name) {
    std::map<std::string, float> snapshot;
    snapshot["master_volume"] = m_controller.get_parameter(parameter_type::MASTER_VOLUME, 0).value;
    snapshot["master_tempo"] = m_controller.get_parameter(parameter_type::MASTER_TEMPO, 0).value;
    snapshot["master_pitch"] = m_controller.get_parameter(parameter_type::MASTER_PITCH, 0).value;

    m_snapshots["global_" + name] = snapshot;
}

void nes_realtime_control_interface::restore_global_snapshot(const std::string& name) {
    auto key = "global_" + name;
    auto it = m_snapshots.find(key);
    if (it != m_snapshots.end()) {
        const auto& snapshot = it->second;

        m_controller.begin_parameter_group();
        for (const auto& param : snapshot) {
            if (param.first == "master_volume") {
                m_controller.set_parameter(parameter_type::MASTER_VOLUME, 0, param.second, "snapshot");
            } else if (param.first == "master_tempo") {
                m_controller.set_parameter(parameter_type::MASTER_TEMPO, 0, param.second, "snapshot");
            } else if (param.first == "master_pitch") {
                m_controller.set_parameter(parameter_type::MASTER_PITCH, 0, param.second, "snapshot");
            }
        }
        m_controller.end_parameter_group();
    }
}

void nes_realtime_control_interface::start_volume_fade(uint8_t channel, float target_volume, std::chrono::milliseconds duration) {
    auto automation = std::make_unique<parameter_automation>(parameter_type::CHANNEL_VOLUME, channel);

    float current_volume = m_controller.get_parameter(parameter_type::CHANNEL_VOLUME, channel).value;
    automation->add_point(std::chrono::milliseconds(0), current_volume);
    automation->add_point(duration, target_volume);
    automation->start();

    m_controller.add_automation(std::move(automation));
}

void nes_realtime_control_interface::start_tempo_ramp(float target_tempo, std::chrono::milliseconds duration) {
    auto automation = std::make_unique<parameter_automation>(parameter_type::MASTER_TEMPO);

    float current_tempo = m_controller.get_parameter(parameter_type::MASTER_TEMPO, 0).value;
    automation->add_point(std::chrono::milliseconds(0), current_tempo);
    automation->add_point(duration, target_tempo);
    automation->start();

    m_controller.add_automation(std::move(automation));
}

void nes_realtime_control_interface::start_filter_sweep(float start_freq, float end_freq, std::chrono::milliseconds duration) {
    auto automation = std::make_unique<parameter_automation>(parameter_type::LOWPASS_CUTOFF);

    automation->add_point(std::chrono::milliseconds(0), start_freq);
    automation->add_point(duration, end_freq);
    automation->start();

    m_controller.add_automation(std::move(automation));
}

void nes_realtime_control_interface::trigger_channel_tremolo(uint8_t channel, float depth, float rate, std::chrono::milliseconds duration) {
    auto automation = std::make_unique<parameter_automation>(parameter_type::CHANNEL_VOLUME, channel);

    float base_volume = m_controller.get_parameter(parameter_type::CHANNEL_VOLUME, channel).value;

    // Create tremolo effect with sine wave
    auto period = std::chrono::milliseconds(static_cast<int>(1000.0f / rate));
    for (auto time = std::chrono::milliseconds(0); time < duration; time += period/4) {
        float phase = static_cast<float>(time.count()) / period.count() * 2.0f * M_PI;
        float tremolo_value = base_volume + depth * std::sin(phase);
        automation->add_point(time, tremolo_value, parameter_automation::curve_type::SINE_WAVE);
    }

    automation->set_loop(true);
    automation->start();

    m_controller.add_automation(std::move(automation));

    // Auto-remove after duration
    // Note: In a real implementation, you'd want to set up a timer to remove this automation
}

void nes_realtime_control_interface::trigger_global_vibrato(float depth, float rate, std::chrono::milliseconds duration) {
    auto automation = std::make_unique<parameter_automation>(parameter_type::MASTER_PITCH);

    // Create vibrato effect
    auto period = std::chrono::milliseconds(static_cast<int>(1000.0f / rate));
    for (auto time = std::chrono::milliseconds(0); time < duration; time += period/8) {
        float phase = static_cast<float>(time.count()) / period.count() * 2.0f * M_PI;
        float vibrato_value = depth * std::sin(phase);
        automation->add_point(time, vibrato_value, parameter_automation::curve_type::SINE_WAVE);
    }

    automation->set_loop(true);
    automation->start();

    m_controller.add_automation(std::move(automation));
}

// MIDI Real-time Handler Implementation
midi_realtime_handler::midi_realtime_handler(realtime_parameter_controller& controller)
    : m_controller(controller) {
    setup_default_mappings();
}

void midi_realtime_handler::process_control_change(uint8_t channel, uint8_t controller, uint8_t value) {
    if (m_learn_mode) {
        // Create new mapping for learned parameter
        midi_mapping mapping(controller, m_learn_parameter, m_learn_channel);
        m_controller.add_midi_mapping(mapping);
        exit_learn_mode();
    }

    m_controller.process_midi_control_change(controller, value);
}

void midi_realtime_handler::process_pitch_bend(uint8_t channel, uint16_t bend_value) {
    // Convert 14-bit pitch bend to normalized value (-1.0 to 1.0)
    float normalized = (static_cast<float>(bend_value) - 8192.0f) / 8192.0f;

    // Apply to master pitch parameter
    m_controller.set_parameter_normalized(parameter_type::MASTER_PITCH, 0, (normalized + 1.0f) / 2.0f, "MIDI PitchBend");
}

void midi_realtime_handler::process_aftertouch(uint8_t channel, uint8_t pressure) {
    // Map aftertouch to channel volume for the specific channel
    if (channel < 5) {  // NES has 5 channels
        float normalized = static_cast<float>(pressure) / 127.0f;
        m_controller.set_parameter_normalized(parameter_type::CHANNEL_VOLUME, channel, normalized, "MIDI Aftertouch");
    }
}

void midi_realtime_handler::process_program_change(uint8_t channel, uint8_t program) {
    // Map program changes to NES-specific parameters
    if (channel < 2) {  // Pulse channels
        uint8_t duty_cycle = program % 4;  // 0-3
        m_controller.set_parameter(parameter_type::PULSE_DUTY_CYCLE, channel, static_cast<float>(duty_cycle), "MIDI Program");
    } else if (channel == 3) {  // Noise channel
        bool short_mode = (program % 2) == 1;
        m_controller.set_parameter(parameter_type::NOISE_MODE, 0, short_mode ? 1.0f : 0.0f, "MIDI Program");
    }
}

void midi_realtime_handler::setup_default_mappings() {
    // Standard MIDI CC mappings
    m_controller.add_midi_mapping(midi_mapping(7, parameter_type::MASTER_VOLUME));      // Volume
    m_controller.add_midi_mapping(midi_mapping(1, parameter_type::MASTER_TEMPO));       // Modulation -> Tempo
    m_controller.add_midi_mapping(midi_mapping(74, parameter_type::LOWPASS_CUTOFF));    // Filter Cutoff
    m_controller.add_midi_mapping(midi_mapping(91, parameter_type::REVERB_AMOUNT));     // Reverb

    // Channel-specific volume controls (CC 7, 8, 9, 10, 11 for channels 0-4)
    for (uint8_t ch = 0; ch < 5; ++ch) {
        m_controller.add_midi_mapping(midi_mapping(7 + ch, parameter_type::CHANNEL_VOLUME, ch));
    }
}

void midi_realtime_handler::setup_nes_specific_mappings() {
    // NES-specific MIDI mappings
    m_controller.add_midi_mapping(midi_mapping(16, parameter_type::PULSE_DUTY_CYCLE, 0));     // Pulse 1 duty
    m_controller.add_midi_mapping(midi_mapping(17, parameter_type::PULSE_DUTY_CYCLE, 1));     // Pulse 2 duty
    m_controller.add_midi_mapping(midi_mapping(18, parameter_type::TRIANGLE_LINEAR_COUNTER)); // Triangle linear counter
    m_controller.add_midi_mapping(midi_mapping(19, parameter_type::NOISE_PERIOD));            // Noise period
    m_controller.add_midi_mapping(midi_mapping(20, parameter_type::NOISE_MODE));              // Noise mode
}

void midi_realtime_handler::enter_learn_mode(parameter_type parameter, uint8_t channel) {
    m_learn_mode = true;
    m_learn_parameter = parameter;
    m_learn_channel = channel;
}

void midi_realtime_handler::exit_learn_mode() {
    m_learn_mode = false;
}

} // namespace nes_realtime