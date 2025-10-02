// Minimal OSD implementation for MAME audio-only operation
#pragma once

#include "emu.h"
#include "osdepend.h"

class minimal_osd_interface : public osd_interface
{
public:
    minimal_osd_interface() = default;
    virtual ~minimal_osd_interface() override = default;

    // General overridables
    virtual void init(running_machine &machine) override {}
    virtual void update(bool skip_redraw) override {}
    virtual void input_update(bool relative_reset) override {}
    virtual void check_osd_inputs() override {}
    virtual void set_verbose(bool print_verbose) override {}

    // Debugger overridables
    virtual void init_debugger() override {}
    virtual void wait_for_debugger(device_t &device, bool firststop) override {}

    // Audio overridables - minimal implementation, we handle audio ourselves
    virtual bool no_sound() override { return false; }
    virtual bool sound_external_per_channel_volume() override { return false; }
    virtual bool sound_split_streams_per_source() override { return false; }
    virtual uint32_t sound_get_generation() override { return 0; }
    virtual osd::audio_info sound_get_information() override { return osd::audio_info{}; }
    virtual uint32_t sound_stream_sink_open(uint32_t node, std::string name, uint32_t rate) override { return 0; }
    virtual uint32_t sound_stream_source_open(uint32_t node, std::string name, uint32_t rate) override { return 0; }
    virtual void sound_stream_close(uint32_t id) override {}
    virtual void sound_stream_sink_update(uint32_t id, const int16_t *buffer, int samples_this_frame) override {}
    virtual void sound_stream_source_update(uint32_t id, int16_t *buffer, int samples_this_frame) override {}
    virtual void sound_stream_set_volumes(uint32_t id, const std::vector<float> &db) override {}
    virtual void sound_begin_update() override {}
    virtual void sound_end_update() override {}

    // Input overridables
    virtual void customize_input_type_list(std::vector<input_type_entry> &typelist) override {}

    // Video overridables
    virtual void add_audio_to_recording(const int16_t *buffer, int samples_this_frame) override {}
    virtual std::vector<ui::menu_item> get_slider_list() override;

    // Font interface
    virtual osd_font::ptr font_alloc() override { return nullptr; }
    virtual bool get_font_families(std::string const &font_path, std::vector<std::pair<std::string, std::string> > &result) override { return false; }

    // Command option overrides
    virtual bool execute_command(const char *command) override { return false; }

    // MIDI interface - not needed for our purposes
    virtual std::unique_ptr<osd::midi_input_port> create_midi_input(std::string_view name) override { return nullptr; }
    virtual std::unique_ptr<osd::midi_output_port> create_midi_output(std::string_view name) override { return nullptr; }
    virtual std::vector<osd::midi_port_info> list_midi_ports() override { return std::vector<osd::midi_port_info>(); }

    // Network interface - not needed
    virtual std::unique_ptr<osd::network_device> open_network_device(int id, osd::network_handler &handler) override { return nullptr; }
    virtual std::vector<osd::network_device_info> list_network_devices() override { return std::vector<osd::network_device_info>(); }
};

// Forward declarations
class emu_options;
class osd_interface;
class machine_manager;

// Factory function to create minimal machine manager (implementation in minimal_osd.cpp)
machine_manager* create_minimal_machine_manager(emu_options &options, osd_interface &osd);

// Cleanup function for osd_interface (which has protected destructor)
void destroy_minimal_osd_interface(osd_interface* osd);
