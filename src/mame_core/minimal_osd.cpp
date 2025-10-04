// Minimal OSD implementation for MAME audio-only operation
#include "emu.h"
#include "minimal_osd.h"
#include "main.h"
#include "http.h"  // For http_manager
#include "ui/uimain.h"  // For ui_manager
#include "synth_modules.h"  // Custom OSD modules
#include "modules/osdmodule.h"
#include "modules/sound/sound_module.h"  // For sound_module
#include <iostream>

// Force linker to include SOUND_CAPTURE module
extern const module_type SOUND_CAPTURE;
static const module_type *force_sound_capture_link = &SOUND_CAPTURE;

// Minimal UI manager for audio-only operation - all methods are stubs
class minimal_ui_manager : public ui_manager
{
public:
    minimal_ui_manager(running_machine &machine) : ui_manager(machine) {}
    virtual ~minimal_ui_manager() = default;

    // Override set_startup_text to avoid crashes - just ignore it
    virtual void set_startup_text(const char *text, bool force) override {
        // Silently ignore startup text in headless mode
    }
};

// Minimal OSD implementation - inherits from osd_common_t
minimal_osd_interface::minimal_osd_interface(osd_options &options)
    : osd_common_t(options)
    , m_audio_callback(nullptr)
{
    std::cout << "Minimal OSD interface created (based on osd_common_t)" << std::endl;
}

minimal_osd_interface::~minimal_osd_interface() {
    std::cout << "Minimal OSD interface destroyed" << std::endl;
}

// sound_stream_sink_update implementation moved below with other sound method overrides

// Override init to set default options before parent initialization
void minimal_osd_interface::init(running_machine &machine) {
    std::cout << "Minimal OSD: init() - setting default options..." << std::endl;

    // Don't set any sound module - our OSD overrides handle audio directly
    // This allows our sound_get_information() override to be called
    const char* sound_option = machine.options().value(OSD_SOUND_PROVIDER);
    std::cout << "  Current sound option: " << (sound_option ? sound_option : "<null>") << std::endl;
    std::cout << "  Using direct OSD audio overrides (no sound module)" << std::endl;

    // Call parent init which will call init_subsystems()
    osd_common_t::init(machine);

    std::cout << "Minimal OSD: init() completed" << std::endl;
}

// Override subsystem initialization to disable unnecessary modules
void minimal_osd_interface::init_subsystems() {
    std::cout << "Minimal OSD: Initializing subsystems (audio-only mode)" << std::endl;

    // Call parent but we'll skip video/input by overriding video_init() and window_init()
    osd_common_t::init_subsystems();

    // Check if sound module was initialized
    if (m_sound) {
        std::cout << "Minimal OSD: Sound module initialized successfully" << std::endl;
    } else {
        std::cout << "Minimal OSD: WARNING - Sound module is NULL!" << std::endl;
    }

    std::cout << "Minimal OSD: Subsystems initialized" << std::endl;
}

// Minimal machine manager for audio-only operation - no UI, no HTTP server, no plugins
class minimal_machine_manager : public machine_manager
{
public:
    minimal_machine_manager(emu_options &options, osd_interface &osd)
        : machine_manager(options, osd)
    {
        // Initialize base class m_http with inactive state (active=false) to avoid crashes
        // http_manager::clear() is called by machine.run() and expects non-null pointer
        // m_http is protected member of machine_manager base class
        m_http = std::make_unique<http_manager>(false, 0, "");
        std::cout << "Minimal machine manager created (no UI/HTTP/plugins)" << std::endl;
    }

    virtual ~minimal_machine_manager() = default;

    // Override create_ui to return our minimal UI manager
    virtual ui_manager* create_ui(running_machine& machine) override {
        return new minimal_ui_manager(machine);
    }
};

// Factory function to create machine manager
machine_manager* create_minimal_machine_manager(emu_options &options, osd_interface &osd) {
    return new minimal_machine_manager(options, osd);
}

// Cleanup function for osd_interface (which has protected destructor)
void destroy_minimal_osd_interface(osd_interface* osd) {
    // Cast to our concrete type which has public destructor
    delete static_cast<minimal_osd_interface*>(osd);
}

// Override video/window subsystem methods to skip video entirely (audio-only mode)
bool minimal_osd_interface::video_init() {
    std::cout << "Minimal OSD: Skipping video initialization (audio-only mode)" << std::endl;
    return true;  // Return success without actually initializing video
}

bool minimal_osd_interface::window_init() {
    std::cout << "Minimal OSD: Skipping window initialization (audio-only mode)" << std::endl;
    return true;  // Return success without actually creating windows
}

void minimal_osd_interface::video_exit() {
    // Nothing to clean up - we never initialized video
}

void minimal_osd_interface::window_exit() {
    // Nothing to clean up - we never created windows
}

void minimal_osd_interface::osd_exit() {
    std::cout << "Minimal OSD: Exiting OSD subsystem" << std::endl;
    // Call parent cleanup if needed
    osd_common_t::osd_exit();
}

// Override input methods (not needed for audio-only operation)
void minimal_osd_interface::input_update(bool relative_reset) {
    // No input processing needed for audio-only mode
}

void minimal_osd_interface::check_osd_inputs() {
    // No OSD inputs to check in audio-only mode
}

// Override event processing methods (not needed for audio-only operation)
void minimal_osd_interface::process_events() {
    // No events to process in headless audio-only mode
}

bool minimal_osd_interface::has_focus() const {
    // Always return true - we're running headless so we always have "focus"
    return true;
}

// Override sound_get_generation() - return static generation number
uint32_t minimal_osd_interface::sound_get_generation() {
    return 1;  // Static generation - our audio config never changes
}

// Override sound_get_information() to provide audio_info with sink nodes
// This is required for newer MAME API - MAmidiMEmo uses older MAME with simpler update_audio_stream()
osd::audio_info minimal_osd_interface::sound_get_information() {
    std::cout << "!!! minimal_osd sound_get_information() called !!!" << std::endl;

    osd::audio_info result;
    result.m_generation = 1;

    // Create a MONO sink node - this tells MAME to create m_osd_output_streams
    // CRITICAL: Must match speaker configuration (we use MONO speaker like MAmidiMEmo)
    osd::audio_info::node_info sink_node;
    sink_node.m_name = "default";
    sink_node.m_display_name = "Audio Capture Sink";
    sink_node.m_id = 1;  // IMPORTANT: Node IDs are 1-based in MAME!

    // Audio rate range - default to 48000Hz for NES APU
    sink_node.m_rate.m_default_rate = 48000;
    sink_node.m_rate.m_min_rate = 8000;
    sink_node.m_rate.m_max_rate = 96000;

    sink_node.m_sinks = 1;  // MONO (matches our MONO speaker)
    sink_node.m_sources = 0;  // No inputs
    sink_node.m_port_names.push_back("Mono");
    sink_node.m_port_positions.push_back(osd::channel_position::FC());  // Front Center

    result.m_nodes.push_back(sink_node);
    result.m_default_sink = 1;  // Match the node ID
    result.m_default_source = 0;

    std::cout << "!!! Returning audio_info with " << result.m_nodes.size() << " nodes !!!" << std::endl;
    if (!result.m_nodes.empty()) {
        auto &node = result.m_nodes[0];
        std::cout << "    Node 0: id=" << node.m_id
                  << ", name=" << node.m_name
                  << ", sinks=" << node.m_sinks
                  << ", sources=" << node.m_sources
                  << ", rate=" << node.m_rate.m_default_rate << std::endl;
    }

    return result;
}

// Override sound_stream_sink_open to create OSD output streams
uint32_t minimal_osd_interface::sound_stream_sink_open(uint32_t node, std::string name, uint32_t rate) {
    static uint32_t next_stream_id = 1;
    uint32_t stream_id = next_stream_id++;

    std::cout << "!!! sound_stream_sink_open: node=" << node
              << ", name=" << name
              << ", rate=" << rate
              << " -> stream_id=" << stream_id << " !!!" << std::endl;

    return stream_id;
}

// Override sound_stream_sink_update - THIS IS WHERE WE GET AUDIO!
void minimal_osd_interface::sound_stream_sink_update(uint32_t id, const int16_t *buffer, int samples_this_frame) {
    static int callback_count = 0;

    // Debug: Check first few callbacks to verify we're getting real audio
    if (callback_count < 10) {
        // Check max sample value in REAL buffer from MAME
        int16_t max_sample = 0;
        for (int i = 0; i < samples_this_frame; i++) {
            if (std::abs(buffer[i]) > std::abs(max_sample)) {
                max_sample = buffer[i];
            }
        }
        std::cerr << "!!! OSD_CALLBACK #" << callback_count
                  << " id=" << id << " samples=" << samples_this_frame
                  << " max=" << max_sample << " (REAL MAME APU) !!!" << std::endl;
        callback_count++;
    }

    // Forward REAL APU audio to our registered callback
    if (m_audio_callback) {
        m_audio_callback(buffer, samples_this_frame);
    }
}

// Override sound_stream_close
void minimal_osd_interface::sound_stream_close(uint32_t id) {
    std::cout << "sound_stream_close: id=" << id << std::endl;
}

// Override sound_external_per_channel_volume - we don't support per-channel volume
bool minimal_osd_interface::sound_external_per_channel_volume() {
    return false;
}

// Override sound_split_streams_per_source - we use single streams
bool minimal_osd_interface::sound_split_streams_per_source() {
    return false;
}

// Override sound_stream_source_open - we don't use source streams (input)
uint32_t minimal_osd_interface::sound_stream_source_open(uint32_t node, std::string name, uint32_t rate) {
    std::cout << "sound_stream_source_open: node=" << node << ", name=" << name << " (not supported)" << std::endl;
    return 0;  // Return 0 = not supported
}

// Override sound_stream_set_volumes - stub (we handle volume elsewhere)
void minimal_osd_interface::sound_stream_set_volumes(uint32_t id, const std::vector<float> &db) {
    // Ignore for now
}

// Override sound_stream_source_update - we don't use source streams
void minimal_osd_interface::sound_stream_source_update(uint32_t id, int16_t *buffer, int samples_this_frame) {
    // Not used - we only have sinks (output), not sources (input)
}

// Override sound_begin_update - called before audio updates
void minimal_osd_interface::sound_begin_update() {
    // Nothing to do
}

// Override sound_end_update - called after audio updates
void minimal_osd_interface::sound_end_update() {
    // Nothing to do
}
