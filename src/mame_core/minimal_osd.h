// Minimal OSD implementation for MAME audio-only operation
#pragma once

#include "emu.h"
#include "modules/lib/osdobj_common.h"

// Minimal OSD interface - inherits from osd_common_t to get proper audio callback infrastructure
// This is the correct approach (same as MAmidiMEmo) instead of reimplementing osd_interface
class minimal_osd_interface : public osd_common_t
{
public:
    minimal_osd_interface(osd_options &options);
    virtual ~minimal_osd_interface() override;

    // Audio capture interface for external access
    using audio_callback_t = std::function<void(const int16_t*, int)>;
    void set_audio_callback(audio_callback_t callback) { m_audio_callback = callback; }

    // Override init to set default sound module
    virtual void init(running_machine &machine) override;

    // Override osd_common_t audio callbacks to capture audio
    virtual void sound_stream_sink_update(uint32_t id, const int16_t *buffer, int samples_this_frame) override;

    // Override subsystem initialization to skip unnecessary modules
    virtual void init_subsystems() override;
    virtual bool video_init() override;
    virtual bool window_init() override;
    virtual void video_exit() override;
    virtual void window_exit() override;
    virtual void osd_exit() override;

    // Override input methods (pure virtual in osd_interface, not needed for audio-only)
    virtual void input_update(bool relative_reset) override;
    virtual void check_osd_inputs() override;

    // Override event processing methods (pure virtual in osd_common_t, not needed for audio-only)
    virtual void process_events() override;
    virtual bool has_focus() const override;

    // Override ALL sound methods to provide audio directly without sound module (newer MAME API)
    // MAmidiMEmo uses older MAME with update_audio_stream(), we use the newer node-based API
    virtual bool sound_external_per_channel_volume() override;
    virtual bool sound_split_streams_per_source() override;
    virtual uint32_t sound_get_generation() override;
    virtual osd::audio_info sound_get_information() override;
    virtual uint32_t sound_stream_sink_open(uint32_t node, std::string name, uint32_t rate) override;
    virtual uint32_t sound_stream_source_open(uint32_t node, std::string name, uint32_t rate) override;
    virtual void sound_stream_set_volumes(uint32_t id, const std::vector<float> &db) override;
    virtual void sound_stream_close(uint32_t id) override;
    // sound_stream_sink_update already declared above (line 23)
    virtual void sound_stream_source_update(uint32_t id, int16_t *buffer, int samples_this_frame) override;
    virtual void sound_begin_update() override;
    virtual void sound_end_update() override;

private:
    audio_callback_t m_audio_callback;
};

// Forward declarations
class emu_options;
class osd_interface;
class machine_manager;

// Factory function to create minimal machine manager (implementation in minimal_osd.cpp)
machine_manager* create_minimal_machine_manager(emu_options &options, osd_interface &osd);

// Cleanup function for osd_interface (which has protected destructor)
void destroy_minimal_osd_interface(osd_interface* osd);

// Helper to initialize sound devices (forward declared running_machine)
class running_machine;
void minimal_initialize_sound_devices(running_machine &machine);
