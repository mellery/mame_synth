// Minimal emulator_info stub for MAME integration
// This provides the bare minimum implementation needed for MAME core

#include <map>
#include <vector>
#include <string>
#include <utility>

// Forward declarations
class running_machine;
class layout_file;

namespace emulator_info {

const char * get_appname() {
    return "mame_synth";
}

const char * get_build_version() {
    return "MAME-Synth 0.1.0";
}

const char * get_bare_build_version() {
    return "0.1.0";
}

const char * get_appname_lower() {
    return "mame_synth";
}

const char * get_configname() {
    return "mame_synth";
}

void periodic_check() {
    // Stub - no periodic checks needed for audio-only
}

bool frame_hook() {
    // Stub - no frame hooks needed for audio-only
    return false;
}

bool draw_user_interface(running_machine&) {
    // Stub - no UI for audio-only
    return false;
}

void sound_hook(std::map<std::string, std::vector<std::pair<float const*, int> > > const&) {
    // Stub - no sound hook needed
}

bool standalone() {
    return true;  // We're standalone, not part of larger MAME
}

void display_ui_chooser(running_machine&) {
    // Stub - no UI chooser
}

void layout_script_cb(layout_file&, const char*) {
    // Stub - no layout scripting needed
}

} // namespace emulator_info
