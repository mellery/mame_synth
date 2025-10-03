// Minimal OSD implementation for MAME audio-only operation
#include "emu.h"
#include "minimal_osd.h"
#include "main.h"                        // For machine_manager
#include "http.h"                        // For http_manager
#include "ui/uimain.h"                   // For ui_manager
#include <iostream>

// Constructor
minimal_osd_interface::minimal_osd_interface(osd_options &options)
    : m_options(options)
    , m_machine(nullptr)
    , m_audio_callback(nullptr)
    , m_verbose(false)
    , m_audio_generation(1)
    , m_sink_stream_id(0)
{
}

// Initialize OSD with running machine
void minimal_osd_interface::init(running_machine &machine) {
    m_machine = &machine;
    std::cout << "Minimal OSD initialized with running_machine" << std::endl;

    // Trigger sound system initialization
    std::cout << "  Triggering sound system setup..." << std::endl;
    machine.sound();  // This should trigger sound_get_information() call
}

// Return audio information
osd::audio_info minimal_osd_interface::sound_get_information() {
    std::cout << "OSD: sound_get_information() called - setting up audio nodes" << std::endl;

    osd::audio_info info;
    info.m_generation = m_audio_generation;
    info.m_default_sink = 1;
    info.m_default_source = 0;

    // Return a single stereo output sink
    osd::audio_info::node_info sink;
    sink.m_id = 1;
    sink.m_name = "audio_output";
    sink.m_display_name = "Audio Output";
    sink.m_sinks = 2; // 2 channels (stereo)
    sink.m_sources = 0;
    sink.m_rate.m_default_rate = 44100;
    sink.m_rate.m_min_rate = 44100;
    sink.m_rate.m_max_rate = 48000;
    info.m_nodes.push_back(sink);

    std::cout << "OSD: Configured " << info.m_nodes.size() << " audio nodes" << std::endl;
    return info;
}

// Open audio sink stream
uint32_t minimal_osd_interface::sound_stream_sink_open(uint32_t node, std::string name, uint32_t rate) {
    m_sink_stream_id++;
    std::cout << "OSD: Opened audio sink stream #" << m_sink_stream_id
              << " (node=" << node << ", name=" << name << ", rate=" << rate << ")" << std::endl;
    return m_sink_stream_id;
}

// Return slider list - implemented inline in header to avoid needing menu.h here

// Audio callback - MAME's sound_manager calls this with mixed audio
void minimal_osd_interface::sound_stream_sink_update(uint32_t id, const int16_t *buffer, int samples_this_frame) {
    // Forward the audio to our callback if one is registered
    if (m_audio_callback) {
        m_audio_callback(buffer, samples_this_frame);
    }

    // Debug output for first few callbacks
    static int callback_count = 0;
    if (callback_count < 5) {
        std::cout << "OSD: Audio callback #" << callback_count
                  << " - stream_id=" << id << ", samples=" << samples_this_frame << std::endl;
        callback_count++;
    }
}

// Minimal machine manager implementation for audio-only operation
class minimal_mame_manager : public machine_manager
{
public:
    minimal_mame_manager(emu_options &options, osd_interface &osd)
        : machine_manager(options, osd)
    {
        // Initialize http_manager with inactive state (active=false)
        // This prevents null pointer crashes but doesn't actually start HTTP server
        m_http = std::make_unique<http_manager>(false, 0, "");
    }

    virtual ~minimal_mame_manager() = default;

    // Override create_ui to provide minimal UI manager
    virtual ui_manager* create_ui(running_machine& machine) override {
        // Create a minimal UI manager and transfer ownership to the machine
        // The machine's m_ui member will own this pointer
        auto *ui = new ui_manager(machine);
        return ui;
    }
};

// Factory function to create minimal machine manager
machine_manager* create_minimal_machine_manager(emu_options &options, osd_interface &osd) {
    return new minimal_mame_manager(options, osd);
}

// Cleanup functions for protected-destructor classes
void destroy_minimal_osd_interface(osd_interface* osd) {
    if (osd) {
        delete static_cast<minimal_osd_interface*>(osd);
    }
}

// Helper to manually initialize devices without calling full run()
// This calls the necessary internal methods to set up the sound subsystem
void minimal_initialize_sound_devices(running_machine &machine) {
    // This function needs access to private methods of running_machine
    // Fortunately, we can use the public sound() accessor and call device start methods
    // through the device interface

    //  The key is that device initialization happens through these calls:
    // 1. sound_manager is lazy-created by sound() accessor
    // 2. before_devices_init() prepares sound system
    // 3. Each device's start() is called (but this is protected)
    // 4. after_devices_init() finalizes sound system

    // Get or create sound manager
    sound_manager &snd = machine.sound();

    // Initialize sound subsystem
    snd.before_devices_init();

    // We can't call start_all_devices() as it's private
    // Instead, let's trigger device initialization through device_reset() which is public
    // This should cause the device to initialize its internal state
    std::cout << "Initializing MAME devices through reset..." << std::endl;
    for (device_t &device : device_enumerator(machine.root_device())) {
        std::cout << "  Resetting device: " << device.tag() << std::endl;
        device.reset();
    }
    std::cout << "MAME devices initialized" << std::endl;

    snd.after_devices_init();
}
