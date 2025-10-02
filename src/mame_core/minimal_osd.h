// Minimal OSD implementation for MAME audio-only operation
#pragma once

#include "emu.h"
#include "modules/lib/osdobj_common.h"

// Inherit from osd_common_t which provides full OSD implementation
// This gives us all the infrastructure MAME expects (rendering, video, etc.)
class minimal_osd_interface : public osd_common_t
{
public:
    minimal_osd_interface(osd_options &options);
    virtual ~minimal_osd_interface() override = default;

    // Pure virtual methods from osd_common_t that we must implement
    virtual void process_events() override {}
    virtual bool has_focus() const override { return true; }

    // Pure virtual methods from osd_interface that we must implement
    virtual void input_update(bool relative_reset) override {}
    virtual void check_osd_inputs() override {}
};

// Forward declarations
class emu_options;
class osd_interface;
class machine_manager;

// Factory function to create minimal machine manager (implementation in minimal_osd.cpp)
machine_manager* create_minimal_machine_manager(emu_options &options, osd_interface &osd);

// Cleanup function for osd_interface (which has protected destructor)
void destroy_minimal_osd_interface(osd_interface* osd);
