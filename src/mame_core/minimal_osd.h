// Minimal OSD implementation for MAME audio-only operation
#pragma once

#include "emu.h"
#include "modules/lib/osdobj_common.h"
#include "ui/menu.h"  // For menu_item - needed for get_slider_list return type

// Minimal OSD interface - implements only what's needed for audio
// We don't inherit from osd_common_t to avoid complex dependencies
class minimal_osd_interface : public osd_interface
{
public:
    minimal_osd_interface(osd_options &options);
    virtual ~minimal_osd_interface() override = default;

    // Audio capture interface for external access
    using audio_callback_t = std::function<void(const int16_t*, int)>;
    void set_audio_callback(audio_callback_t callback) { m_audio_callback = callback; }

    // osd_interface required methods - general
    virtual void init(running_machine &machine) override;
    virtual void update(bool skip_redraw) override {}
    virtual void input_update(bool relative_reset) override {}
    virtual void check_osd_inputs() override {}
    virtual void set_verbose(bool print_verbose) override { m_verbose = print_verbose; }

    // debugger overridables
    virtual void init_debugger() override {}
    virtual void wait_for_debugger(device_t &device, bool firststop) override {}

    // audio overridables - THIS IS WHERE WE CAPTURE AUDIO
    virtual bool no_sound() override { return false; }
    virtual bool sound_external_per_channel_volume() override { return false; }
    virtual bool sound_split_streams_per_source() override { return false; }
    virtual uint32_t sound_get_generation() override { return m_audio_generation; }
    virtual osd::audio_info sound_get_information() override;
    virtual uint32_t sound_stream_sink_open(uint32_t node, std::string name, uint32_t rate) override;
    virtual uint32_t sound_stream_source_open(uint32_t node, std::string name, uint32_t rate) override { return 0; }
    virtual void sound_stream_close(uint32_t id) override {}
    virtual void sound_stream_sink_update(uint32_t id, const int16_t *buffer, int samples_this_frame) override;
    virtual void sound_stream_source_update(uint32_t id, int16_t *buffer, int samples_this_frame) override {}
    virtual void sound_stream_set_volumes(uint32_t id, const std::vector<float> &db) override {}
    virtual void sound_begin_update() override {}
    virtual void sound_end_update() override {}

    // input overridables
    virtual void customize_input_type_list(std::vector<input_type_entry> &typelist) override {}

    // video/recording overridables
    virtual void add_audio_to_recording(const int16_t *buffer, int samples_this_frame) override {}
    virtual std::vector<ui::menu_item> get_slider_list() override {
        // Return empty vector - menu_item is forward declared only so we can't construct it
        // But we can return an empty vector of the incomplete type
        std::vector<ui::menu_item> empty;
        return empty;
    }

    // font interface
    virtual osd_font::ptr font_alloc() override { return nullptr; }
    virtual bool get_font_families(std::string const &font_path, std::vector<std::pair<std::string, std::string>> &result) override { return false; }

    // command option overrides
    virtual bool execute_command(const char *command) override { return false; }

    // MIDI interface
    virtual std::unique_ptr<osd::midi_input_port> create_midi_input(std::string_view name) override { return nullptr; }
    virtual std::unique_ptr<osd::midi_output_port> create_midi_output(std::string_view name) override { return nullptr; }
    virtual std::vector<osd::midi_port_info> list_midi_ports() override { return {}; }

    // network interface
    virtual std::unique_ptr<osd::network_device> open_network_device(int id, osd::network_handler &handler) override { return nullptr; }
    virtual std::vector<osd::network_device_info> list_network_devices() override { return {}; }

private:
    osd_options &m_options;
    running_machine *m_machine = nullptr;
    audio_callback_t m_audio_callback;
    bool m_verbose = false;
    uint32_t m_audio_generation = 1;
    uint32_t m_sink_stream_id = 0;
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
