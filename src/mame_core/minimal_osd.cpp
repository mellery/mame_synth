// Minimal OSD implementation for MAME audio-only operation
#include "emu.h"
#include "minimal_osd.h"
#include "frontend/mame/ui/menuitem.h"  // For ui::menu_item complete type
#include "main.h"                        // For machine_manager

std::vector<ui::menu_item> minimal_osd_interface::get_slider_list() {
    return {};  // Return empty vector
}

// Minimal machine manager implementation for audio-only operation
class minimal_mame_manager : public machine_manager
{
public:
    minimal_mame_manager(emu_options &options, osd_interface &osd)
        : machine_manager(options, osd)
    {
    }

    virtual ~minimal_mame_manager() = default;
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
