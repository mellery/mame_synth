// Minimal OSD implementation for MAME audio-only operation
#include "emu.h"
#include "minimal_osd.h"
#include "main.h"                        // For machine_manager
#include "http.h"                        // For http_manager
#include "ui/uimain.h"                   // For ui_manager

// Constructor - pass options to osd_common_t base class
minimal_osd_interface::minimal_osd_interface(osd_options &options)
    : osd_common_t(options)
{
    // Base class handles initialization
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

    // We need to start all devices, but device_t::start() is protected
    // The proper way is through running_machine::start_all_devices() which is private
    // So we need a friend-like workaround...

    // Actually, let's try calling device methods through the device_execute_interface
    // Or we could iterate and manually configure each device...

    // For now, let's try iterating through devices and configuring them
    for (device_t &device : device_enumerator(machine.root_device())) {
        // Each device will have already been configured during machine_config construction
        // The issue is calling device_start() which is protected
        // We can't call it from here without being a friend
    }

    snd.after_devices_init();
}
